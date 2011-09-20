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
    return TRUE;
  }

  private function ProcessRetsClass(dRealtyConnectionEntity $connection, $resource, $class, $entity_type) {

    $query_fields = array();
    $offset = 0;
    $props = array();
    $mls_field = NULL;
    $price_field = NULL;

    $mappings = $connection->ResourceMappings();
    $resources = $connection->Resources();

    // build a list of fields we are going to request from the RETS server
    $fieldmappings = $connection->FetchFieldMappings($resource, $class->cid);


    drush_log(dt("Processing @res", array("@res" => $resource)));
    if (!$class->override_status_query) {
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
    } else {
      $query = array();
      drush_log(dt("using @var", array("@var" => $class->override_status_query_text)));
      $query[] = $class->override_status_query_text;
    }



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
            'Offset' => "$offset",
            'RestrictedIndicator' => 'xxxx',
            'Count' => '1',
        );
        // do the actual search
        $search = $this->dc->get_phrets()->SearchQuery($resource, $class->systemname, "($q)", $optional_params);
        $items = array();
        // loop through the search results
        while ($item = $this->dc->get_phrets()->FetchRow($search)) {
          // calculate the hash
          $item['hash'] = $this->calculate_hash($item);
          $items[] = $item;
        }
        if ($error = $this->dc->get_phrets()->Error()) {
          drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
        }
        drush_log(dt("caching @count items for resource: @resource | class: @class", array("@count" => count($items), "@resource" => $resource, "@class" => $class->systemname)));
        cache_set("drealty_chunk_{$resource}_{$class->systemname}_" . $chunks++, $items);

        $offset += count($items) + 1;
        if ($limit == 'NONE') {
          $end = FALSE;
        } else {
          $end = $this->dc->get_phrets()->IsMaxrowsReached();
        }
        $this->dc->get_phrets()->FreeResult($search);
      }
      $this->dc->disconnect();

      // do some cleanup
      unset($items, $query_fields, $offset, $mls_field, $price_field, $mappings, $resources);

      // at this point we have data waiting to be processed. Need to process the
      // data which will insert/update/delete the listing data as nodes
      drush_log(dt("process_results( connection: @connection_name, resource: @resource, class: @class, chunks: @chunks)", array("@connection_name" => $connection->name, "@resource" => $resource,
            "@class" => $class->systemname, "@chunks" => $chunks)));
      $this->process_results($connection, $resource, $class, $entity_type, $chunks);
      $this->process_images($connection, $resource);
    } else {
      $error = $this->dc->get_phrets()->Error();
      watchdog('drealty', "drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), WATCHDOG_ERROR);
      drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
    }
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


    $query = new EntityFieldQuery();
    $result = $query->entityCondition('entity_type', $entity_type, '=')
      ->propertyCondition('conid', $connection->conid)
      ->execute();

    $existing_items_tmp = array();
    if (!empty($result)) {
      $existing_items_tmp = entity_load($entity_type, array_keys($result[$entity_type]));
    }

    switch ($entity_type) {
      case 'drealty_listing':
      case 'drealty_openhouse':
        $key_field = 'listing_key';
        break;
      case 'drealty_agent':
        $key_field = 'agent_id';
        break;
      case 'drealty_office':
        $key_field = 'office_id';
        break;
    }

    //re-key the array to use the ListingKey 
    $existing_items = array();
    foreach ($existing_items_tmp as $existing_item_tmp) {
      $existing_items[$existing_item_tmp->{$key_field}] = $existing_item_tmp;
    }

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

        if (!isset($existing_items[$rets_item[$id]]) || $existing_items[$rets_item[$id]]->hash != $rets_item['hash']) {

          $item = new Entity(array('conid' => $connection->conid), $entity_type);

          // this listing either doesn't exist in the IDX or has changed. 
          // determine if we need to update or create a new one.
          if (isset($existing_items[$rets_item[$id]])) {
            // this listing exists so we'll get a reference to it and set the values to what came to us in the RETS result
            $item = &$existing_items[$rets_item[$id]];
          } else {
            $item->created = time();
          }

          $item->conid = $connection->conid;
          $item->name = $rets_item[$id];
          $item->hash = $rets_item['hash'];
          $item->changed = time();
          $item->class = $class->cid;
          $item->rets_imported = TRUE;

          if ($entity_type == 'drealty_listing') {
            $item->process_images = FALSE;
          }

          foreach ($field_mappings as $mapping) {
            if (isset($rets_item[$mapping->systemname])) {

              $value = '';

              switch ($schema_fields[$mapping->field_name]['type']) {
                case 'varchar':
                case 'char':
                  $value = substr($rets_item[$mapping->systemname], 0, $schema_fields[$mapping->field_name]['length']);
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
                      drush_log($string);
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
            $street_number = isset($item->street_number) ? $item->street_number : '';
            $street_name = isset($item->street_name) ? $item->street_name : '';
            $street_suffix = isset($item->street_suffix) ? $item->street_suffix : '';

            $geoaddress = "{$street_number} {$street_name} {$street_suffix}, {$item->city}, {$item->state_or_province} {$item->postal_code}";
            // remove any double spaces
            $geoaddress = str_replace("  ", "", $geoaddress);

            if ($latlon = drealty_geocode($geoaddress)) {
              if ($latlon->success) {
                $item->latitude = $latlon->lat;
                $item->longitude = $latlon->lon;
                drush_log(dt('Geocoded: @address to (@lat, @lon)', array('@address' => $geoaddress, '@lat' => $item->latitude, '@lon' => $item->longitude)));
              } else {
                drush_log(dt('Failed to Geocode: @address)', array('@address' => $geoaddress)));
              }
            } else {
              drush_log(dt('Failed to Geocode: @address)', array('@address' => $geoaddress)));
            }
          }

          try {
            $item->save();
          } catch (Exception $e) {
            drush_log($e->getMessage());
          }
          drush_log(dt('Saving item @name', array('@name' => $item->name)));
          unset($item);
        } else {
          // skipping this item
          drush_log(dt("Skipping item @name", array("@name" => $rets_item[$id])));
        }
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
      $tmp .= strtolower(trim($value));
    }
    return md5($tmp);
  }

  public function process_images($conid, $resource) {
    $entity_type = 'drealty_listing';
    $chunk_size = 25;

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

          $photos = $this->dc->get_phrets()->GetObject($resource, "Photo", $id_string, '*');

          if ($this->dc->get_phrets()->Error()) {
            $error = $this->dc->get_phrets()->Error();
            drush_log($error['text']);
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

