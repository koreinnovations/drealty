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
   * @var drealtyMetaData 
   */
  protected $dm;

  /**
   *
   * @var DrupalQueueInterface
   */
  protected $queue;

  public function __construct() {
    $this->dc = new drealtyConnection();
    $this->dm = new drealtyMetaData();
    $this->queue = DrupalQueue::get('drealty');
    $this->queue->deleteQueue();
  }

  public function run() {
    $connections = $this->dc->FetchConnections();
    foreach ($connections as $connection) {
      $mappings = $connection->ResourceMappings();
      foreach ($mappings as $mapping) {
        $resource = $this->dm->FetchResource($mapping->rid);
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

  public function import_images() {
    $connections = $this->dc->FetchConnections();
    foreach ($connections as $connection) {
      $mappings = $connection->ResourceMappings();
      foreach ($mappings as $mapping) {
        $resource = $this->dm->FetchResource($mapping->rid);
        $classes = $connection->FetchClasses($resource);
        foreach ($classes as $class) {
          if ($class->enabled && $class->process_images) {
            $this->process_images($connection->conid, $resource, $class);
          }
        }
      }
    }

    unset($connections, $mappings, $classes);
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
        $chunks = $this->fetch_listings_offset_not_supported_price($connection, $resource, $class, $key_field);
        break;
      case 2:
        $query = array();
        drush_log(dt("using @var", array("@var" => $class->override_status_query_text)));
        $query[] = $class->override_status_query_text;
        $this->fetch_listings_offset_supported_default($connection, $resource, $class, $query);
        break;
      case 3:
        $this->fetch_listings_offset_not_supported_key($connection, $resource, $class, $key_field);
        break;
      case 0:
      default:
//build the query
        $statuses = $class->status_values;
        $status_q = "|$statuses";
        $query_field = 'rets_status';
        $query = array();
        $query[] = "{$fieldmappings[$query_field]->systemname}={$status_q}";
        $this->fetch_listings_offset_supported_default($connection, $resource, $class, $query);
    }

// at this point we have data waiting to be processed. Need to process the
// data which will insert/update/delete the listing data as nodes
    drush_log(dt("process_results( connection: @connection_name, resource: @resource, class: @class, chunks: @chunks)", array("@connection_name" => $connection->name, "@resource" => $resource->systemname, "@class" => $class->systemname, "@chunks" => $chunks)));
    $this->process_results($connection, $resource, $class, $entity_type);
    if ($entity_type == 'drealty_listing' && $class->process_images) {
      $this->process_images($connection->conid, $resource, $class);
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

      if ($class->process_images) {
        $options['Select'] .= ',' . $class->photo_timestamp_field;
      }

      $options['Limit'] = $limit;


      while ($count <= $total) {

        $search = $rets->SearchQuery($resource->systemname, $class->systemname, $query, $options);

        if ($rets->NumRows($search) > 0) {
          while ($listing = $rets->FetchRow($search)) {
            $listing['hash'] = $this->calculate_hash($listing, $connection->conid, $class->cid);

            $this->queue->createItem($listing);

            $count++;
          }

          ksort($listings);
          $last = end($listings);
          reset($listings);

          $id = $last[$key_field];
          $id = (int) $id + 1;

          unset($listings);
        } else {
          break;
        }

        $rets->FreeResult($search);
        drush_log("[connection: {$connection->name}][resource: {$resource->systemname}][class: {$class->systemname}][downloaded: $count][query: $query]");

        $query = "({$key_field}={$id}+)";
      }
      $this->dc->disconnect();
    } else {
      $error = $rets->Error();
      watchdog('drealty', "drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), WATCHDOG_ERROR);
      drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
    }
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

      $options['Select'] = $this->get_fields($connection->conid, $class->cid);

      if ($class->process_images) {
        $options['Select'] .= ',' . $class->photo_timestamp_field;
      }

      while ($count < $total) {

        $search = $rets->SearchQuery($resource->systemname, $class->systemname, $offset_query, $options);

        if ($rets->NumRows($search) > 0) {
          while ($listing = $rets->FetchRow($search)) {
            $listing['hash'] = $this->calculate_hash($listing, $connection->conid, $class->cid);

            $this->queue->createItem($listing);

            $count++;
          }
        }

        $rets->FreeResult($search);
        drush_log("Resource: {$resource->systemname} Class: {$class->systemname} Listings Downloaded: $count Query: $offset_query");

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
    $count = 0;
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

        if ($class->process_images) {
          $fields .= ',' . $class->photo_timestamp_field;
        }

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

        // loop through the search results
        while ($listing = $rets->FetchRow($search)) {
          // calculate the hash
          $listing['hash'] = $this->calculate_hash($listing, $connection->conid, $class->cid);

          $this->queue->createItem($listing);
          $count++;
        }


        if ($error = $rets->Error()) {
          drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
        }

        drush_log(dt("Queuing @count items for resource: @resource | class: @class", array("@count" => $count, "@resource" => $resource->systemname, "@class" => $class->systemname)));


        $offset += $count;

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
  }

  public function update_single_listing(DrealtyListing $listing) {
    $connection = $this->dc->FetchConnection($listing->conid);
    $class = $this->dm->FetchClass($listing->class);
    $resource = $this->dm->FetchResource($class->rid);

    $field_mappings = $connection->FetchFieldMappings($resource, $class);
    $key_field = $field_mappings['rets_key']->systemname;

    $item_context = array('field_mappings' => $field_mappings, 'connection' => $connection, 'resource' => $resource, 'key_field' => $key_field);

    $query = "({$key_field}={$listing->rets_key})";

    $fields = $this->get_fields($connection->conid, $class->cid);

    if ($class->process_images) {
      $fields .= ',' . $class->photo_timestamp_field;
    }

    $params = array(
      'Format' => 'COMPACT-DECODED',
      'Limit' => "1",
      'RestrictedIndicator' => 'xxxx',
      'Count' => '0',
      'Select' => $fields,
    );

    if ($this->dc->connect($connection->conid)) {
      $rets = $this->dc->rets;
      $search = $rets->SearchQuery($resource->systemname, $class->systemname, $query, $params);

      $item = $rets->FetchRow($search);

      $rets->FreeResult($search);

      $listing->hash = $this->calculate_hash($item, $connection->conid, $class->cid);
      $listing->changed = time();


      $this->set_field_data($listing, $item, $field_mappings, $listing->entityType(), $class, TRUE);
      $item_context['rets_item'] = $item;


      if ($class->process_images) {
        //get the images
        $results = $rets->GetObject($resource->systemname, $class->object_type, $listing->rets_key, '*');
        $img_dir = file_default_scheme() . "://{$class->image_dir}";
        $img_field = $class->image_field_name;

        //sort the images
        if ($results) {
          $photos = array();
          foreach ($results as $item) {
            $photos[$item['Object-ID']] = $item;
          }
          ksort($photos, SORT_NUMERIC);

          unset($listing->{$img_field}[LANGUAGE_NONE]);

          foreach ($photos as $photo) {

            $mlskey = $photo['Content-ID'];
            $number = $photo['Object-ID'];
            $filename = "{$mlskey}-{$number}.jpg";
            $filepath = "{$img_dir}/{$filename}";
            //ensure that there is enough data to actually make a file.
            if (strlen($photo['Data']) > 173) {
              $file = file_save_data($photo['Data'], $filepath, FILE_EXISTS_REPLACE);
              //make sure we actually save the image
              if ($file) {
                $file->alt = '';
                $file->title = '';
                $listing->{$img_field}[LANGUAGE_NONE][] = (array) $file;
              }
            }
          }
        }
      }

      try {
        drupal_alter('drealty_import_presave', $listing, $item_context);
        $listing->save();
        module_invoke_all('drealty_entity_save', array(&$listing, $item_context));
      } catch (Exception $e) {
        watchdog("drealty", $e->getMessage(), array(), WATCHDOG_ERROR);
        drupal_set_message($e->getMessage(), 'error');
        return FALSE;
      }
      $this->dc->disconnect();
      return TRUE;
    }
    return FALSE;
  }

  /**
   *
   * @param drealtyConnectionEntity $connection
   * @param drealtyRetsResource $resource
   * @param drealtyRetsClass $class
   * @param string $entity_type
   * @param int $chunk_count 
   */
  protected function process_results(drealtyConnectionEntity $connection, $resource, $class, $entity_type) {

    $in_rets = array();

    $key_field = 'rets_key';

    $existing_items = db_select($entity_type, "t")
      ->fields("t", array($key_field, "hash", "id"))
      ->condition("conid", $connection->conid)
      ->condition('class', $class->cid)
      ->execute()
      ->fetchAllAssoc($key_field);

// get the fieldmappings
    $field_mappings = $connection->FetchFieldMappings($resource, $class);

// set $id to the systemname of the entity's corresponding key from the rets feed to make the code easier to read
    $id = $field_mappings[$key_field]->systemname;

    $item_context = array('field_mappings' => $field_mappings, 'connection' => $connection, 'resource' => $resource, 'key_field' => $key_field);
    $total = $this->queue->numberOfItems();
    $count = 1;
    while ($queue_item = $this->queue->claimItem()) {

      $rets_item = $queue_item->data;


      $in_rets[$rets_item[$id]] = $rets_item[$id];

      $force = FALSE;
      if (!isset($existing_items[$rets_item[$id]]) || $existing_items[$rets_item[$id]]->hash != $rets_item['hash'] || $force) {

        $is_new = TRUE;
        $item = entity_create($entity_type, array('conid' => $connection->conid, 'type' => $class->bundle));

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
          if ($is_new) {
            $item->process_images = TRUE;
            $item->rets_photo_modification_timestamp = $rets_item[$class->photo_timestamp_field];
          } else {
            if (isset($item->rets_photo_modification_timestamp)) {
              $last_time = strtotime($item->rets_photo_modification_timestamp);
              $this_time = strtotime($rets_item[$class->photo_timestamp_field]);
              if ($this_time > $last_time) {
                $item->process_images = TRUE;
              } else {
                $item->process_images = FALSE;
              }
            } else {
              // hasn't been set but it's not new
              $item->rets_photo_modification_timestamp = $rets_item[$class->photo_timestamp_field];
              $item->process_images = FALSE;
            }
          }
        }

        $force_geocode = FALSE;

        if (!$is_new) {
          $geofields = $this->dm->FetchFieldMappings($connection->conid, $resource, $class, 'geofield');
          if (!empty($geofields)) {
            foreach ($geofields as $geofield) {
              // check to see if we already have already geocoded this address
              if (!isset($item->{$geofield->field_name}[LANGUAGE_NONE][0]['lat']) && !isset($item->{$geofield->field_name}[LANGUAGE_NONE][0]['lon'])) {
                $force_geocode = TRUE;
              }
            }
          }
        } else {
          $force_geocode = TRUE;
        }

        $this->set_field_data($item, $rets_item, $field_mappings, $entity_type, $class, $force_geocode);

        $item_context['rets_item'] = $rets_item;

        try {
          drupal_alter('drealty_import_presave', $item, $item_context);
          $item->save();
          module_invoke_all('drealty_entity_save', array(&$item, $item_context));
          $this->queue->deleteItem($queue_item);
        } catch (Exception $e) {
          drush_log($e->getMessage());
          $this->queue->releaseItem($queue_item);
        }
        drush_log(dt('Saving item @name. [@count of @total]', array("@name" => $rets_item[$id], "@count" => $count, "@total" => $total)));
        unset($item);
      } else {
        // skipping this item
        drush_log(dt("Skipping item @name. [@count of @total]", array("@name" => $rets_item[$id], "@count" => $count, "@total" => $total)));
        $this->queue->deleteItem($queue_item);
      }
      $count++;
      drupal_get_messages();
      drupal_static_reset();
    }
    //handle expired listings
    $this->handle_expired($in_rets, $connection->conid, $class);
  }

  /**
   * Function to handle the logic of what to do with expired listings
   * 
   * @param array $in_rets
   * @param array $conid
   * @param drealtyRetsClass $class 
   */
  protected function handle_expired($in_rets, $conid, $class) {

    $results = db_select('drealty_listing', 'dl')
      ->fields('dl', array('id', 'rets_key'))
      ->condition('conid', $conid)
      ->condition('class', $class->cid)
      ->execute()
      ->fetchAllAssoc('rets_key');

    $diff = array_diff_key($results, $in_rets);

    foreach ($diff as $item) {
      switch ($class->expired_handler) {
        case 0:
          $listing = drealty_listing_load($item->id);
          $listing->delete();
          break;
        case 1:
          db_update('drealty_listing')
            ->fields(array('active' => 0))
            ->condition('id', $item->id)
            ->execute();
          break;
        default:
          $listing = drealty_listing_load($item->id);
          $listing->delete();
      }
    }
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

    /* grab any address fields so we can set changed = false
     * doing this so that geocoder wont re-geocode an address field 
     * on entity->save().
     * 
     * There's got to be a better way to do this, however, this will work for now.
     */



    $address_fields = $this->dm->FetchFieldMappings($conid, $resource, $class, 'addressfield');

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

          $results = $rets->GetObject($resource->systemname, $class->object_type, $id_string, '*');

          if ($rets->Error()) {
            $error = $rets->Error();
            drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text'])), 'error');
            return;
          }

          if (!empty($results)) {
            $length = 0;
            $total = 0;
            $photos = array();

            foreach ($results as $item) {
              if ($item['Success'] == TRUE) {
                if (strlen($item['Data']) > 173) {
                  $total++;
                  $length += strlen($item['Data']);

                  if (!isset($photos[$item['Content-ID']])) {
                    $photos[$item['Content-ID']] = array();
                  }
                  $photos[$item['Content-ID']][$item['Object-ID']] = $item;
                } else {
                  drush_log(dt("Images failed to download: @code - @text", array('@code' => $item['ReplyCode'], '@text' => $item['ReplyText'])), 'warning');
                }
              }
            }

            ksort($photos, SORT_NUMERIC);
            foreach ($photos as &$set) {
              ksort($set, SORT_NUMERIC);
            }
            drush_log(dt("Downloaded a total of @total images for @count Listings.", array("@total" => $total, "@count" => count($ids))));
          } else {
            drush_log(dt("GetObject for @resource - @class returned an empty result set.", array('@resource' => $resource->systemname, '@class' => $class->systemname)), 'warning');
          }

          $this->dc->disconnect();

          unset($ids, $results);

          $id_string = "";
          $counter = 0;

          foreach ($photos as $list_id => $set) {

            $listing = $listings[$list_id];

            // delete out any existing images
            if (isset($listing->{$img_field}[LANGUAGE_NONE])) {
              foreach ($listing->{$img_field}[LANGUAGE_NONE] as $key => $file) {
                $image = file_load($file['fid']);
                unset($listing->{$img_field}[LANGUAGE_NONE][$key]);
                if (!empty($image)) {
                  file_delete($image);
                }
              }
            }

            foreach ($set as $key => $photo) {

              $mlskey = $photo['Content-ID'];
              $number = $photo['Object-ID'];
              $filename = "{$mlskey}-{$number}.jpg";
              $filepath = "{$img_dir}/{$filename}";
              //ensure that there is enough data to actually make a file.
              if (strlen($photo['Data']) > 173) {
                if ($file = file_save_data($photo['Data'], $filepath, FILE_EXISTS_REPLACE)) {
                  //make sure we actually save the image
                  $file->alt = '';
                  $file->title = '';
                  $listing->{$img_field}[LANGUAGE_NONE][] = (array) $file;
                }
              }
            }

            $listing->process_images = 0;

            if (!empty($address_fields)) {
              // set each address field's changed = FALSE
              foreach ($address_fields as $address_field) {
                $listing->{$address_field->field_name}[LANGUAGE_NONE][0]['changed'] = FALSE;
              }
              reset($address_fields);
            }

            $listing->save();
            drush_log(dt("Saved @count images for @listing", array("@count" => count($set), "@listing" => $list_id)), "success");
            unset($photos[$list_id], $listings[$list_id]);
            drupal_get_messages();
            drupal_static_reset();
          }
          // apparently some mls feeds have listings with 0 images, set them as processed
          foreach ($listings as $listing) {
            $listing->process_images = 0;
            if (!empty($address_fields)) {
              // set each address field's changed = FALSE
              foreach ($address_fields as $address_field) {
                $listing->{$address_field->field_name}[LANGUAGE_NONE][0]['changed'] = FALSE;
              }
              reset($address_fields);
            }
            $listing->save();
            drush_log(dt("Listing @id had no images. Marking as processed.", array("@id" => $list_id)), 'warning');
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

  protected function set_field_data(&$item, $rets_item, $field_mappings, $entity_type, $class, $force_geocode = FALSE) {

    foreach ($field_mappings as $mapping) {
      switch ($mapping->field_api_type) {
        case 'addressfield':
          if ($force_geocode) {
            $item->{$mapping->field_name}[LANGUAGE_NONE][0]['changed'] = TRUE;
          } else {
            $item->{$mapping->field_name}[LANGUAGE_NONE][0]['changed'] = FALSE;
          }
          //get the default country code if one exists for the address
          $field_info = field_info_instance($entity_type, $mapping->field_name, $class->bundle);
          $item->{$mapping->field_name}[LANGUAGE_NONE][0]['country'] = isset($field_info['default_value'][0]['country']) ? $field_info['default_value'][0]['country'] : 'US';
          $item->{$mapping->field_name}[LANGUAGE_NONE][0]['thoroughfare'] = isset($rets_item[$mapping->data['address_1']]) ? $rets_item[$mapping->data['address_1']] : NULL;
          $item->{$mapping->field_name}[LANGUAGE_NONE][0]['premise'] = isset($mapping->data['address_2']) ? $rets_item[$mapping->data['address_2']] : NULL;
          $item->{$mapping->field_name}[LANGUAGE_NONE][0]['locality'] = isset($mapping->data['city']) ? $rets_item[$mapping->data['city']] : NULL;
          $item->{$mapping->field_name}[LANGUAGE_NONE][0]['administrative_area'] = isset($mapping->data['state']) ? $rets_item[$mapping->data['state']] : NULL;
          $item->{$mapping->field_name}[LANGUAGE_NONE][0]['sub_administrative_area'] = isset($mapping->data['county']) ? $rets_item[$mapping->data['county']] : NULL;
          $item->{$mapping->field_name}[LANGUAGE_NONE][0]['postal_code'] = isset($mapping->data['zip']) ? $rets_item[$mapping->data['zip']] : NULL;
          break;
        case 'geofield':
          $item->{$mapping->field_name}[LANGUAGE_NONE][0]['wkt'] = GEOCODER_DUMMY_WKT;
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
  }

}
