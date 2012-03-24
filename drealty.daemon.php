<?php

define('GEOCODER_DUMMY_WKT', 'POINT(0, 0)');

class drealtyDaemon {

  /**
   *
   * @var drealtyConnection 
   */
  protected $dc;

  /**
   *
   * @var drealtyResources 
   */
  protected $dr;

  public function __construct() {
    $this->dc = new drealtyConnection();
    $this->dr = new drealtyResources();
  }

  public function run() {
    $connections = $this->dc->FetchConnections();
    foreach ($connections as $connection) {
      $mappings = $connection->ResourceMappings();
      foreach ($mappings as $mapping) {
        $resource = $this->dr->FetchResource($mapping->rid);
        $classes = $connection->FetchClasses($resource);
        foreach ($classes as $class) {
          if ($class->enabled && $class->lifetime <= time() - ($class->lastupdate + 60)) {
            $this->ProcessRetsClass($connection, $resource, $class, $mapping->entity_type);
            $class->lastupdate = time();
            drupal_write_record('drealty_classes', $class, 'cid');
          }
        }
      }
    }

    unset($connections, $mappings, $classes);
    cache_clear_all();
    module_invoke_all('drealty_rets_import_complete');
    return TRUE;
  }

  /**
   *
   * @param drealtyConnectionEntity $connection
   * @param drealtyRetsResource $resource
   * @param drealtyRetsClass $class
   * @param string $entity_type 
   */
  private function ProcessRetsClass(drealtyConnectionEntity $connection, $resource, $class, $entity_type) {

    $mappings = $connection->ResourceMappings();
    $resources = $connection->Resources();
    $key_field = "";
    $chunks = 0;

// build a list of fields we are going to request from the RETS server
    $fieldmappings = $connection->FetchFieldMappings($resource, $class);

    drush_log(dt("Processing Resource: @res for Connection: @con", array("@res" => $resource->systemname, "@con" => $connection->name)));

    $key_field = $fieldmappings['rets_key']->systemname;

    switch ($class->query_type) {
      case 1:
        drush_log("Key Field: $key_field");
        $chunks = $this->fetch_listings_offset_not_supported_price($connection, $resource, $class, $key_field);
        break;
      case 2:
        $query = array();
        drush_log(dt("using @var", array("@var" => $class->override_status_query_text)));
        $query[] = $class->override_status_query_text;
        $chunks = $this->fetch_listings_offset_supported_default($connection, $resource, $class, $query);
        break;
      case 3:
        $chunks = $this->fetch_listings_offset_not_supported_key($connection, $resource, $class, $key_field);
        break;
      case 0:
      default:
//build the query
        $statuses = $class->status_values;
        $status_q = "|$statuses";

        switch ($entity_type) {
          case 'drealty_listing':
          case 'drealty_openhouse':
            $query_field = 'rets_status';
            break;
          case 'drealty_agent':
          case 'drealty_office':
            $query_field = 'type';
            break;
          default:
            $query_field = 'rets_status';
        }
        $query = array();
        $query[] = "{$fieldmappings[$query_field]->systemname}={$status_q}";
        $chunks = $this->fetch_listings_offset_supported_default($connection, $resource, $class, $query);
    }

// at this point we have data waiting to be processed. Need to process the
// data which will insert/update/delete the listing data as nodes
    drush_log(dt("process_results( connection: @connection_name, resource: @resource, class: @class, chunks: @chunks)", array("@connection_name" => $connection->name, "@resource" => $resource->systemname, "@class" => $class->systemname, "@chunks" => $chunks)));
    $this->process_results($connection, $resource, $class, $entity_type, $chunks);
    if ($entity_type == 'drealty_listing' && $class->process_images) {
      $this->process_images($connection, $resource, $class);
    }

    unset($mappings, $resources, $fieldmappings, $query);
  }

  /**
   *
   * @param drealtyConnectionEntity $connection
   * @param drealtyRetsResource $resource
   * @param drealtyRetsClass $class
   * @param string $key_field
   * @return int 
   */
  function fetch_listings_offset_not_supported_key(drealtyConnectionEntity $connection, $resource, $class, $key_field) {
    $rets = $this->dc->rets;

    $chunks = 0;
    $id = 0;

    $query = "({$key_field}={$id}+)";
    $limit = $class->chunk_size;

    $options = array(
      'count' => 1,
      'Format' => 'COMPACT-DECODED',
      'Select' => $key_field,
    );


    if ($this->dc->connect($connection->conid)) {


      $search = $rets->SearchQuery($resource->systemname, $class->systemname, $query, $options);

      if ($error = $rets->Error()) {
        drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
      }

      $total = $rets->TotalRecordsFound($search);
      $rets->FreeResult($search);

      drush_log(dt("Total Listings @total", array('@total' => $total)));

      $count = 0;
      $listings = array();

      $options['Select'] = $this->get_fields($connection->conid, $class->cid);

      $options['Limit'] = $limit;


      while ($count <= $total) {

        $search = $rets->SearchQuery($resource->systemname, $class->systemname, $query, $options);

        if ($rets->NumRows($search) > 0) {
          while ($listing = $rets->FetchRow($search)) {
            $listing['hash'] = $this->calculate_hash($listing, $connection->conid, $class->cid);
            $listings[] = $listing;
            $count++;
          }

          ksort($listings);
          $last = end($listings);
          reset($listings);

          $id = $last[$key_field];
          $id = (int) $id + 1;

          cache_set("drealty_chunk_{$resource->systemname}_{$class->systemname}_" . $chunks++, $listings);
          unset($listings);
        } else {
          break;
        }

        $rets->FreeResult($search);
        drush_log("[connection: {$connection->name}][resource: {$resource->systemname}][class: {$class->systemname}][downloaded: $count][query: $query][chunks: $chunks]");

        $query = "({$key_field}={$id}+)";
      }
      $this->dc->disconnect();
    } else {
      $error = $rets->Error();
      watchdog('drealty', "drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), WATCHDOG_ERROR);
      drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
    }
    unset($listings);
    return $chunks;
  }

  /**
   *
   * @param drealtyConnectionEntity $connection
   * @param drealtyRetsResource $resource
   * @param drealtyRetsClass $class
   * @param string $key_field
   * @return int 
   */
  function fetch_listings_offset_not_supported_price(drealtyConnectionEntity $connection, $resource, $class, $key_field) {
    $rets = $this->dc->rets;

    $chunks = 0;
    $offset_amount = $class->offset_amount;
    $offset_max = $class->offset_max;
    $offset_start = 0;
    $offset_end = $offset_start + $offset_amount;

    $query = $class->override_status_query_text;
    $options = array(
      'count' => 1,
      'Format' => 'COMPACT-DECODED',
      'Select' => $key_field,
    );


    if ($this->dc->connect($connection->conid)) {


      $search = $rets->SearchQuery($resource->systemname, $class->systemname, $query, $options);

      if ($error = $rets->Error()) {
        drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
      }

      $total = $rets->TotalRecordsFound($search);
      $rets->FreeResult($search);

      $offset_query = "$query,({$class->offset_field}={$offset_start}-{$offset_end})";
      $count = 0;
      $listings = array();

      $options['Select'] = $this->get_fields($connection->conid, $class->cid);

      while ($count < $total) {

        $search = $rets->SearchQuery($resource->systemname, $class->systemname, $offset_query, $options);

        if ($rets->NumRows($search) > 0) {
          while ($listing = $rets->FetchRow($search)) {
            $listing['hash'] = $this->calculate_hash($listing, $connection->conid, $class->cid);
            $listings[] = $listing;
            $count++;
          }
          cache_set("drealty_chunk_{$resource->systemname}_{$class->systemname}_" . $chunks++, $listings);
          unset($listings);
        }

        $rets->FreeResult($search);
        drush_log("Resource: {$resource->systemname} Class: {$class->systemname} Listings Downloaded: $count Query: $offset_query  Chunks: $chunks");

        if ($offset_end < $offset_max) {
          $offset_start = $offset_end + 1;
          $offset_end += $offset_amount;
          $offset_query = "$query,({$class->offset_field}={$offset_start}-{$offset_end})";
        } else {
          $offset_query = "$query,({$class->offset_field}={$offset_max}+)";
        }
      }
      $this->dc->disconnect();
    } else {
      $error = $rets->Error();
      watchdog('drealty', "drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), WATCHDOG_ERROR);
      drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
    }
    unset($listings);
    return $chunks;
  }

  /**
   *
   * @param drealtyConnectionEntity $connection
   * @param drealtyRetsResource $resource
   * @param drealtyRetsClass $class
   * @param string $query
   * @return int 
   */
  function fetch_listings_offset_supported_default(drealtyConnectionEntity $connection, $resource, $class, $query) {
    $offset = 0;
    $rets = $this->dc->rets;
    $limit = $class->chunk_size;
    if ($limit == 0) {
      $limit = 'NONE';
    }
    $chunks = 0;

    if ($this->dc->connect($connection->conid)) {
      // prepare the query
      $q = implode('),(', $query);

      $end = TRUE;

      // fetch the search results until we've queried for them all
      while ($end) {
        $end_p = $end ? "FALSE" : "TRUE";
        drush_log("Resource: {$resource->systemname} Class: $class->systemname Limit: $limit Offset: $offset MaxRowsReached: $end_p Chunks: $chunks");

        $result = db_select('drealty_field_mappings', 'dfm')
          ->fields('dfm')
          ->condition('conid', $connection->conid)
          ->condition('cid', $class->cid)
          ->execute()
          ->fetchAllAssoc('systemname');

        $fields = $this->get_fields($connection->conid, $class->cid);


        $optional_params = array(
          'Format' => 'COMPACT-DECODED',
          'Limit' => "$limit",
          'RestrictedIndicator' => 'xxxx',
          'Count' => '1',
          'Select' => $fields,
          'Offset' => $offset,
        );

        // do the actual search
        $search = $rets->SearchQuery($resource->systemname, $class->systemname, "($q)", $optional_params);

        $items = array();

        // loop through the search results
        while ($item = $rets->FetchRow($search)) {
          // calculate the hash
          $listing['hash'] = $this->calculate_hash($listing, $connection->conid, $class->cid);
          $items[] = $item;
        }


        if ($error = $rets->Error()) {
          drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
        }

        drush_log(dt("caching @count items for resource: @resource | class: @class", array("@count" => count($items), "@resource" => $resource->systemname, "@class" => $class->systemname)));
        cache_set("drealty_chunk_{$resource->systemname}_{$class->systemname}_" . $chunks++, $items);

        $offset += count($items);

        if ($limit == 'NONE') {
          $end = FALSE;
        } else {
          $end = $rets->IsMaxrowsReached();
        }
        $rets->FreeResult($search);
      }
      $this->dc->disconnect();

// do some cleanup
      unset($items);
    } else {
      $error = $rets->Error();
      watchdog('drealty', "drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), WATCHDOG_ERROR);
      drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
    }
    return $chunks;
  }

  /**
   *
   * @param drealtyConnectionEntity $connection
   * @param drealtyRetsResource $resource
   * @param drealtyRetsClass $class
   * @param string $entity_type
   * @param int $chunk_count 
   */
  protected function process_results(drealtyConnectionEntity $connection, $resource, $class, $entity_type, $chunk_count) {

    drush_log('processing results');
    $chunk_idx = 0;
    $in_rets = array();

    $key_field = 'rets_key';

    $existing_items = db_select($entity_type, "t")
      ->fields("t", array($key_field, "hash", "id"))
      ->condition("conid", $connection->conid)
      ->execute()
      ->fetchAllAssoc($key_field);

// get the fieldmappings
    $field_mappings = $connection->FetchFieldMappings($resource, $class);

// set $id to the systemname of the entity's corresponding key from the rets feed to make the code easier to read
    $id = $field_mappings[$key_field]->systemname;

    $item_context = array('field_mappings' => $field_mappings, 'connection' => $connection, 'resource' => $resource, 'key_field' => $key_field);

    for ($i = 0; $i < $chunk_count; $chunk_idx++, $i++) {
      $chunk_name = "drealty_chunk_{$resource->systemname}_{$class->systemname}_{$chunk_idx}";
      $rets_results = cache_get($chunk_name);

      $rets_results_count = count($rets_results->data);

      for ($j = 0; $j < $rets_results_count; $j++) {


        drush_log(dt("Item @idx of @total", array("@idx" => $j + 1, "@total" => $rets_results_count)));
        $rets_item = $rets_results->data[$j];
        $in_rets[] = $rets_item[$id];

        $force = FALSE;
        if (!isset($existing_items[$rets_item[$id]]) || $existing_items[$rets_item[$id]]->hash != $rets_item['hash'] || $force) {

          $is_new = TRUE;
          $item = new Entity(array('conid' => $connection->conid, 'type' => $class->bundle), $entity_type);

          // this listing either doesn't exist in the IDX or has changed. 
          // determine if we need to update or create a new one.
          if (isset($existing_items[$rets_item[$id]])) {
            // this listing exists so we'll get a reference to it and set the values to what came to us in the RETS result
            $item = reset(entity_load($entity_type, array($existing_items[$rets_item[$id]]->id)));
            $is_new = FALSE;
          } else {
            $item->created = time();
          }

          $item->conid = $connection->conid;

          $item->hash = $rets_item['hash'];
          $item->changed = time();
          $item->class = $class->cid;
          $item->rets_imported = TRUE;

          if ($entity_type == 'drealty_listing' && $class->process_images) {
            $item->process_images = TRUE;
          }

          foreach ($field_mappings as $mapping) {
            switch ($mapping->field_api_type) {
              case 'addressfield':
                $item->{$mapping->field_name}[LANGUAGE_NONE][0]['changed'] = FALSE;

                if (
                  isset($mapping->data['address_1']) &&
                  isset($rets_item[$mapping->data['address_1']]) &&
                  isset($item->{$mapping->field_name}[LANGUAGE_NONE][0]['thoroughfare']) &&
                  $rets_item[$mapping->data['address_1']] != $item->{$mapping->field_name}[LANGUAGE_NONE][0]['thoroughfare']) {
                  $item->{$mapping->field_name}[LANGUAGE_NONE][0]['changed'] = TRUE;
                }
                if (
                  isset($mapping->data['address_2']) &&
                  isset($rets_item[$mapping->data['address_2']]) &&
                  isset($item->{$mapping->field_name}[LANGUAGE_NONE][0]['premise']) &&
                  $rets_item[$mapping->data['address_2']] != $item->{$mapping->field_name}[LANGUAGE_NONE][0]['premise']) {
                  $item->{$mapping->field_name}[LANGUAGE_NONE][0]['changed'] = TRUE;
                }
                if (
                  isset($mapping->data['city']) &&
                  isset($rets_item[$mapping->data['city']]) &&
                  isset($item->{$mapping->field_name}[LANGUAGE_NONE][0]['locality']) &&
                  $rets_item[$mapping->data['city']] != $item->{$mapping->field_name}[LANGUAGE_NONE][0]['locality']) {
                  $item->{$mapping->field_name}[LANGUAGE_NONE][0]['changed'] = TRUE;
                }
                if (
                  isset($mapping->data['state']) &&
                  isset($rets_item[$mapping->data['state']]) &&
                  isset($item->{$mapping->field_name}[LANGUAGE_NONE][0]['administrative_area']) &&
                  $rets_item[$mapping->data['state']] != $item->{$mapping->field_name}[LANGUAGE_NONE][0]['administrative_area']) {
                  $item->{$mapping->field_name}[LANGUAGE_NONE][0]['changed'] = TRUE;
                }
                if (
                  isset($mapping->data['zip']) &&
                  isset($rets_item[$mapping->data['zip']]) &&
                  isset($item->{$mapping->field_name}[LANGUAGE_NONE][0]['postal_code']) &&
                  $rets_item[$mapping->data['zip']] != $item->{$mapping->field_name}[LANGUAGE_NONE][0]['postal_code']) {
                  $item->{$mapping->field_name}[LANGUAGE_NONE][0]['changed'] = TRUE;
                }
                if($is_new) {
                  $item->{$mapping->field_name}[LANGUAGE_NONE][0]['changed'] = TRUE;
                }
                
                $item->{$mapping->field_name}[LANGUAGE_NONE][0]['thoroughfare'] = isset($rets_item[$mapping->data['address_1']]) ? $rets_item[$mapping->data['address_1']] : NULL;
                $item->{$mapping->field_name}[LANGUAGE_NONE][0]['premise'] = isset($mapping->data['address_2']) ? $rets_item[$mapping->data['address_2']] : NULL;
                $item->{$mapping->field_name}[LANGUAGE_NONE][0]['locality'] = isset($mapping->data['city']) ? $rets_item[$mapping->data['city']] : NULL;
                $item->{$mapping->field_name}[LANGUAGE_NONE][0]['administrative_area'] = isset($mapping->data['state']) ? $rets_item[$mapping->data['state']] : NULL;
                $item->{$mapping->field_name}[LANGUAGE_NONE][0]['postal_code'] = isset($mapping->data['zip']) ? $rets_item[$mapping->data['zip']] : NULL;

                break;
              case 'geofield':
                // check to see if we already have already geocoded this address
                if (!isset($item->{$mapping->field_name}[LANGUAGE_NONE][0]['lat']) && !isset($item->{$mapping->field_name}[LANGUAGE_NONE][0]['lon'])) {
                  $item->{$mapping->field_name}[LANGUAGE_NONE][0]['wkt'] = GEOCODER_DUMMY_WKT;
                  $item->{$mapping->field_name}[LANGUAGE_NONE][0]['geocode'] = TRUE;
                }
                break;
              case 'text_long':
                $item->{$mapping->field_name}[LANGUAGE_NONE][0]['value'] = $rets_item[$mapping->systemname];
                $item->{$mapping->field_name}[LANGUAGE_NONE][0]['format'] = 'plain_text';
                break;
              case 'number_integer':
              case 'number_decimal':
              case 'number_float':
                $item->{$mapping->field_name}[LANGUAGE_NONE][0]['value'] = empty($rets_item[$mapping->systemname]) ? 0 : is_numeric($rets_item[$mapping->systemname]) ? $rets_item[$mapping->systemname] : 0;
                break;
              case 'list_boolean':
                $item->{$mapping->field_name}[LANGUAGE_NONE][0]['value'] = in_array($rets_item[$mapping->systemname], array('true', 'True', 'TRUE', 'yes', 'Yes', 'y', 'Y', '1', 'on', 'On', 'ON', true, 1), true) ? 1 : 0;
                break;
              case 'drealty':
                $item->{$mapping->field_name} = $rets_item[$mapping->systemname];
                break;
              default:
                $item->{$mapping->field_name}[LANGUAGE_NONE][0]['value'] = $rets_item[$mapping->systemname];
            }
          }


          try {
            drupal_alter('drealty_import_presave', $item, $item_context);
            $item->save();
            module_invoke_all('drealty_entity_save', array(&$item, $item_context));
          } catch (Exception $e) {
            drush_log($e->getMessage());
          }
          drush_log(dt('Saving item @name', array('@name' => $rets_item[$id])));
          unset($item);
        } else {
          // skipping this item
          drush_log(dt("Skipping item @name", array("@name" => $rets_item[$id])));
        }

        /*
         *  TODO: sold or removed listings. || removed agents || removed offices
         * 
         *  deal with items that are no longer returned from the rets feed but are still in the db. we can either
         *  delete them, archive/unpublish them, or come up with a status field. should be some sort of option
         *  as each realtor association will have a different rule concerning if a listing can be displayed after it 
         *  has been sold or removed from the feed.
         * 
         *  
         * 
         */
      }
      cache_clear_all($chunk_name, 'cache');
    } // endfor $chunk_count
  }

  /**
   * Calculate an md5 hash on the resulting listing used to determine if we need
   * to perform an update
   *
   * @param array $listing
   * @return string
   */
  protected function calculate_hash(array $items, $connection_id, $class_id) {

    $cache = &drupal_static(__FUNCTION__);

    if (empty($cache[$connection_id]) || empty($cache[$connection_id][$class_id])) {
      $field_mappings = db_select('drealty_field_mappings', 'dfm')
        ->fields('dfm')
        ->condition('conid', $connection_id)
        ->condition('cid', $class_id)
        ->condition('hash_exclude', FALSE)
        ->execute()
        ->fetchAll();

      $cache[$connection_id][$class_id] = $field_mappings;
    }

    $fields = $cache[$connection_id][$class_id];

    $tmp = '';
    foreach ($fields as $field) {
      switch ($field->field_api_type) {
        case 'addressfield':
          $data = unserialize($field->data);
          foreach ($data as $item) {
            $tmp .= drupal_strtolower(trim($items[$item]));
          }
          break;
        case 'geofield':
          // in the case of a geofield we don't have anything to map to, so we'll skip it.
          break;
        default:
          $tmp .= drupal_strtolower(trim($items[$field->systemname]));
      }
    }
    return md5($tmp);
  }

  /**
   *
   * @global type $user
   * @param int $conid
   * @param drealtyRetsResource $resource
   * @param drealtyRetsClass $class
   * @return type 
   */
  public function process_images($conid, $resource, $class) {

    if (!$class->process_images) {
      return;
    }
    global $user;
    $img_field = $class->image_field_name;
    $dir = $class->image_dir;

    $rets = $this->dc->rets;
    $entity_type = 'drealty_listing';
    $chunk_size = $class->image_chunk_size;

    $query = new EntityFieldQuery();
    $result = $query
      ->entityCondition('entity_type', $entity_type)
      ->propertyCondition('process_images', 1)
      ->execute();

    if (!empty($result[$entity_type])) {
      $items = array_keys($result[$entity_type]); //entity_load($entity_type, array_keys($result[$entity_type]));
    } else {
      drush_log("No images to process.");
      return;
    }

    //make sure we have something to process
    if (count($items) >= 1) {
      drush_log("process_images() - Starting.");
      $img_dir = file_default_scheme() . "://$dir";

      if (!file_prepare_directory($img_dir, FILE_MODIFY_PERMISSIONS | FILE_CREATE_DIRECTORY)) {
        drush_log(dt("Failed to create %directory.", array('%directory' => $img_dir)), "error");
        return;
      } else {
        if (!is_dir($img_dir)) {
          drush_log(dt("Failed to locate %directory.", array('%directory' => $img_dir)), "error");
          return;
        }
      }


      $process_array = array_chunk($items, $chunk_size, TRUE);

      foreach ($process_array as $chunk) {

        $listings = entity_load($entity_type, $chunk);
        
        $ids = array();
        $rekeyed_listings = array();
        foreach ($listings as $key => $listing) {
          $ids[] = $listing->rets_key;
          $listings[$listing->rets_key] = $listing;
          unset($listings[$key]);
        }


        if ($this->dc->connect($conid)) {
          $id_string = implode(',', $ids);
          drush_log("id string: " . $id_string);

          $photos = $rets->GetObject($resource->systemname, $class->object_type, $id_string, '*');

          if ($rets->Error()) {
            $error = $rets->Error();
            drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
            return;
          }

          $this->dc->disconnect();

          unset($ids);
          $id_string = "";
          $counter = 0;

          foreach ($photos as $photo) {
            $mlskey = $photo['Content-ID'];
            $number = $photo['Object-ID'];
            $filename = "{$mlskey}-{$number}.jpg";
            $filepath = "{$img_dir}/{$filename}";


            $fid = db_query('SELECT fid FROM {file_managed} WHERE filename = :filename', array(':filename' => $filename))->fetchField();

            if (!empty($fid)) {
              $file_object = file_load($fid);
              file_delete($file_object, TRUE);
            }

            drush_log(dt("Saving @filename", array("@filename" => $filepath)));

            $file = file_save_data($photo['Data'], $filepath, FILE_EXISTS_REPLACE);
            $file->alt = '';
            $file->title = '';

            $listing = $listings[$mlskey];

            $listing->{$img_field}[LANGUAGE_NONE][] = (array) $file;

            $listing->process_images = 0;
            $listing->save();
            
            unset($listing, $file);
          }
          unset($photos, $listings);
        }
      }
    }
    cache_clear_all("prop_images_to_process", "cache");
  }

  protected function get_fields($conid, $class_id) {
    $results = db_select('drealty_field_mappings', 'dfm')
      ->fields('dfm')
      ->condition('conid', $conid)
      ->condition('cid', $class_id)
      ->execute();


    $fields = array();
    foreach ($results as $result) {
      switch ($result->field_api_type) {
        case 'addressfield':
          $data = unserialize($result->data);
          foreach ($data as $item) {
            $fields[] = $item;
          }
          break;
        case 'geofield':
          // in the case of a geofield we don't have anything to map to, so we'll skip it.
          break;
        default:
          $fields[] = $result->systemname;
      }
    }
    return implode(',', $fields);
  }

}