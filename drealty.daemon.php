<?php

class drealtyDaemon {

  protected $dc;
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
        $classes = $connection->FetchClasses($mapping->resource);
        foreach ($classes as $class) {
          if ($class->enabled && $class->lifetime <= time() - ($class->lastupdate + 60)) {
            $this->ProcessRetsClass($connection, $mapping->resource, $class, $mapping->entity_type);
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

  private function ProcessRetsClass(dRealtyConnectionEntity $connection, $resource, $class, $entity_type) {

    $mappings = $connection->ResourceMappings();
    $resources = $connection->Resources();
    $key_field = "";
    $chunks = 0;

    // build a list of fields we are going to request from the RETS server
    $fieldmappings = $connection->FetchFieldMappings($resource, $class->cid);

    drush_log(dt("Processing Resource: @res for Connection: @con", array("@res" => $resource, "@con" => $connection->name)));

    switch ($entity_type) {
      case 'drealty_listing':
      case 'drealty_openhouse':
        $key_field = $fieldmappings['listing_key']->systemname;
        break;
      case 'drealty_agent':
        $key_field = $fieldmappings['agent_key']->systemname;
        break;
      case 'drealty_office':
        $key_field = $fieldmappings['office_key']->systemname;
        break;
    }

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
            $query_field = 'listing_status';
            break;
          case 'drealty_agent':
          case 'drealty_office':
            $query_field = 'type';
            break;
          default:
            $query_field = 'listing_status';
        }
        $query = array();
        $query[] = "{$fieldmappings[$query_field]->systemname}={$status_q}";
        $chunks = $this->fetch_listings_offset_supported_default($connection, $resource, $class, $query);
    }

    // at this point we have data waiting to be processed. Need to process the
    // data which will insert/update/delete the listing data as nodes
    drush_log(dt("process_results( connection: @connection_name, resource: @resource, class: @class, chunks: @chunks)", array("@connection_name" => $connection->name, "@resource" => $resource, "@class" => $class->systemname, "@chunks" => $chunks)));
    $this->process_results($connection, $resource, $class, $entity_type, $chunks);
    if ($entity_type == 'drealty_listing' && $class->process_images) {
      $this->process_images($connection, $resource, $class);
    }

    unset($mappings, $resources, $fieldmappings, $query);
  }

  function fetch_listings_offset_not_supported_key(dRealtyConnectionEntity $connection, $resource, $class, $key_field) {
    $rets = &$this->dc->get_phrets();

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


      $search = $rets->SearchQuery($resource, $class->systemname, $query, $options);

      if ($error = $rets->Error()) {
        drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
      }

      $total = $rets->TotalRecordsFound($search);
      $rets->FreeResult($search);

      drush_log(dt("Total Listings @total", array('@total' => $total)));

      $count = 0;
      $listings = array();

      unset($options['Select']);
      $options['Limit'] = $limit;


      while ($count <= $total) {

        $search = $rets->SearchQuery($resource, $class->systemname, $query, $options);

        if ($rets->NumRows($search) > 0) {
          while ($listing = $rets->FetchRow($search)) {
            $listing['hash'] = $this->calculate_hash($listing);
            $listings[] = $listing;
            $count++;
          }

          ksort($listings);
          $last = end($listings);
          reset($listings);

          $id = $last[$key_field];
          $id = (int) $id + 1;

          cache_set("drealty_chunk_{$resource}_{$class->systemname}_" . $chunks++, $listings);
          unset($listings);
        } else {
          break;
        }

        $rets->FreeResult($search);
        drush_log("[connection: {$connection->name}][resource: $resource][class: {$class->systemname}][downloaded: $count][query: $query][chunks: $chunks]");

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

  function fetch_listings_offset_not_supported_price(dRealtyConnectionEntity $connection, $resource, $class, $key_field) {
    $rets = &$this->dc->get_phrets();

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


      $search = $rets->SearchQuery($resource, $class->systemname, $query, $options);

      if ($error = $rets->Error()) {
        drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
      }

      $total = $rets->TotalRecordsFound($search);
      $rets->FreeResult($search);

      $offset_query = "$query,({$class->offset_field}={$offset_start}-{$offset_end})";
      $count = 0;
      $listings = array();

      unset($options['Select']);

      while ($count < $total) {

        $search = $rets->SearchQuery($resource, $class->systemname, $offset_query, $options);

        if ($rets->NumRows($search) > 0) {
          while ($listing = $rets->FetchRow($search)) {
            $listing['hash'] = $this->calculate_hash($listing);
            $listings[] = $listing;
            $count++;
          }
          cache_set("drealty_chunk_{$resource}_{$class->systemname}_" . $chunks++, $listings);
          unset($listings);
        }

        $rets->FreeResult($search);
        drush_log("Resource: $resource Class: {$class->systemname} Listings Downloaded: $count Query: $offset_query  Chunks: $chunks");

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

  function fetch_listings_offset_supported_default(dRealtyConnectionEntity $connection, $resource, $class, $query) {
    $offset = 0;
    $rets = &$this->dc->get_phrets();
    $limit = $class->chunk_size;
    if ($limit == 0) {
      $limit = 'NONE';
    }
    $chunks = 0;

    if ($this->dc->connect($connection->conid)) {
      // prepare the query
      $q = implode('),(', $query);
      // $fields = implode(',', $query_fields);

      $end = TRUE;

      // fetch the search results until we've queried for them all
      while ($end) {
        $end_p = $end ? "FALSE" : "TRUE";
        drush_log("Resource: $resource Class: $class->systemname Limit: $limit Offset: $offset MaxRowsReached: $end_p Chunks: $chunks");


        $optional_params = array(
          'Format' => 'COMPACT-DECODED',
          'Limit' => "$limit",
          'RestrictedIndicator' => 'xxxx',
          'Count' => '1',
          'Offset' => $offset,
        );

        // do the actual search
        $search = $rets->SearchQuery($resource, $class->systemname, "($q)", $optional_params);

        $items = array();

        // loop through the search results
        while ($item = $rets->FetchRow($search)) {
          // calculate the hash
          $item['hash'] = $this->calculate_hash($item);
          $items[] = $item;
        }


        if ($error = $rets->Error()) {
          drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
        }

        drush_log(dt("caching @count items for resource: @resource | class: @class", array("@count" => count($items), "@resource" => $resource, "@class" => $class->systemname)));
        cache_set("drealty_chunk_{$resource}_{$class->systemname}_" . $chunks++, $items);

        $offset += count($items) + 1;

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
   * Insert / Update the listing data
   *
   * @param int $conid
   *  The dRealty connection id to be processed
   * @param string $resource
   *  The RETS resource to be processed
   * @param string $class
   *  The RETS class to be processed
   * @param string $type
   *  The associated node type to be processed
   * @param array $context_mapping
   *  A mapping array for id fields:
   *  array("id"=>int, "id_field"=>int);
   * @param int $chunk_count
   *  The number of chunks that need to be processed
   *
   */
  protected function process_results(dRealtyConnectionEntity $connection, $resource, $class, $entity_type, $chunk_count) {

    // $first_run = variable_get("drealty_connection_{$connection->conid}_first_run", TRUE);

    $schema = drupal_get_schema_unprocessed("drealty", $entity_type);
    $schema_fields = $schema['fields'];

    drush_log('processing results');
    $chunk_idx = 0;
    $in_rets = array();


    switch ($entity_type) {
      case 'drealty_listing':
      case 'drealty_openhouse':
        $key_field = 'listing_key';
        break;
      case 'drealty_agent':
        $key_field = 'agent_key';
        break;
      case 'drealty_office':
        $key_field = 'office_key';
        break;
    }

    $existing_items = db_select($entity_type, "t")
      ->fields("t", array($key_field, "hash", "id"))
      ->condition("conid", $connection->conid)
      ->execute()
      ->fetchAllAssoc($key_field);

    // get the fieldmappings
    $field_mappings = $connection->FetchFieldMappings($resource, $class->cid);

    // set $id to the systemname of the entity's corresponding key from the rets feed to make the code easier to read
    $id = $field_mappings[$key_field]->systemname;

    for ($i = 0; $i < $chunk_count; $chunk_idx++, $i++) {
      $chunk_name = "drealty_chunk_{$resource}_{$class->systemname}_{$chunk_idx}";
      $rets_results = cache_get($chunk_name);

      $rets_results_count = count($rets_results->data);

      for ($j = 0; $j < $rets_results_count; $j++) {


        drush_log(dt("Item @idx of @total", array("@idx" => $j + 1, "@total" => $rets_results_count)));
        $rets_item = $rets_results->data[$j];
        $in_rets[] = $rets_item[$id];

        $force = FALSE;
        if (!isset($existing_items[$rets_item[$id]]) || $existing_items[$rets_item[$id]]->hash != $rets_item['hash'] || $force) {

          $is_new = TRUE;
          $item = new Entity(array('conid' => $connection->conid), $entity_type);

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
          if ($entity_type == 'drealty_listing') {
            $item->name = $rets_item[$id];
          }
          $item->hash = $rets_item['hash'];
          $item->changed = time();
          $item->class = $class->cid;
          $item->rets_imported = TRUE;

          if ($entity_type == 'drealty_listing' && $class->process_images) {
            $item->process_images = TRUE;
          }

          foreach ($field_mappings as $mapping) {
            if (isset($rets_item[$mapping->systemname])) {

              $value = '';

              switch ($schema_fields[$mapping->field_name]['type']) {
                case 'varchar':
                case 'char':
                  $value = drupal_substr($rets_item[$mapping->systemname], 0, $schema_fields[$mapping->field_name]['length']);
                  break;
                case 'integer':
                case 'float':
                case 'decimal':
                case 'numeric':
                case 'int':
                  $string = $rets_item[$mapping->systemname];
                  switch ($mapping->field_name) {
                    case 'end_datetime':
                    case 'start_datetime':
                    case 'listing_date':
                    case 'status_changed_datetime':
                      $value = strtotime($string);
                      break;
                    default:
                      $val = preg_replace('/[^0-9\.]/Uis', '', $string);
                      $value = is_numeric($val) ? $val : 0;
                  }
                  break;
                default:
                  $value = $rets_item[$mapping->systemname];
              }
              $item->{$mapping->field_name} = $value;
            }
          }

          if ($class->do_geocoding) {

            $geoaddress = isset($item->street_number) ? $item->street_number : '';
            $geoaddress .= isset($item->street_dir_prefix) ? $item->street_dir_prefix : '';
            $geoaddress .= isset($item->street_name) ? $item->street_name : '';
            $geoaddress .= isset($item->street_dir_suffix) ? $item->street_dir_suffix : '';
            $geoaddress .= isset($item->street_suffix) ? $item->street_suffix : '';
            $geoaddress .= ", {$item->city}, {$item->state_or_province} {$item->postal_code}";

            // remove any double spaces
            $geoaddress = str_replace("  ", "", $geoaddress);

            if ($latlon = drealty_geocode($geoaddress, $class->geocoder_handler)) {
              if ($latlon->success) {
                $item->latitude = $latlon->lat;
                $item->longitude = $latlon->lon;
                drush_log(dt('Geocoded: @address to (@lat, @lon)', array('@address' => $geoaddress, '@lat' => $item->latitude, '@lon' => $item->longitude)));
              } else {
                drush_log(dt('Failed to Geocode: @address', array('@address' => $geoaddress)));
              }
            } else {
              drush_log(dt('There was a failure with the Geocoder. Please check the configuration for the handler: @handler', array('@handler' => $class->geocoder_handler)));
            }
          }

          try {
            module_invoke_all('drealty_import_presave', $item);
            $item->save();
            module_invoke_all('drealty_import_save', $item);
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
  protected function calculate_hash(array $item) {
    $tmp = '';
    foreach ($item as $key => $value) {
      $tmp .= drupal_strtolower(trim($value));
    }
    return md5($tmp);
  }

  public function process_images($conid, $resource, $class) {
    $rets = &$this->dc->get_phrets();
    $entity_type = 'drealty_listing';
    $chunk_size = $class->image_chunk_size;

    $query = new EntityFieldQuery();
    $result = $query
      ->entityCondition('entity_type', $entity_type)
      ->propertyCondition('process_images', TRUE)
      ->execute();

    if (!empty($result[$entity_type])) {
      $items = entity_load($entity_type, array_keys($result[$entity_type]));
    } else {
      drush_log("No images to process.");
      return;
    }

    //make sure we have something to process
    if (count($items) >= 1) {
      drush_log("process_images() - Starting.");
      $img_dir = file_default_scheme() . '://drealty_image';

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

        $ids = array();

        foreach ($chunk as $item) {
          $ids[] = $item->listing_key;
        }

        if ($this->dc->connect($conid)) {
          $id_string = implode(',', $ids);
          drush_log("id string: " . $id_string);

          $rets_result = $rets->GetObject($resource, $class->object_type, $id_string, '*');



          if ($rets->Error()) {
            $error = $rets->Error();
            drush_log($error['text']);
            return;
          }

          $this->dc->disconnect();


          $photos = array();
          foreach ($rets_result as $photo) {
            $photos[$photo['Object-ID']] = $photo;
          }

          ksort($photos);

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
            // load the entity that is associated with the image
            $query = new EntityFieldQuery();
            $result = $query
              ->entityCondition('entity_type', 'drealty_listing')
              ->propertyCondition('listing_key', $mlskey)
              ->execute();
            $listing = reset(entity_load('drealty_listing', array_keys($result['drealty_listing']), array(), FALSE));

            file_usage_add($file, 'drealty', $entity_type, $listing->id);

            $listing->process_images = 0;
            $listing->save();
          }
          unset($photos);
        }
      }
    }
    cache_clear_all("prop_images_to_process", "cache");
  }

}

