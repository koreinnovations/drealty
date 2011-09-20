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
          if ($class->enabled && $class->lifetime <= time() - ($class->lastupdate + 120)) {
            $this->ProcessRetsClass($connection, $mapping->resource, $class, $mapping->node_type);
            $class->lastupdate = time();
            drupal_write_record('drealty_classes', $class, 'cid');
          }
        }
      }
    }
    unset($coneections, $mappings, $classes);
    cache_clear_all();
    return TRUE;
  }

  private function ProcessRetsClass(dRealtyConnectionEntity $connection, $resource, $class) {

    $query_fields = array();
    $offset = 0;
    $props = array();
    $mls_field = NULL;
    $price_field = NULL;

    $mappings = $connection->ResourceMappings();
    $resources = $connection->Resources();

    // build a list of fields we are going to request from the RETS server
    $fieldmappings = $connection->FetchFieldMappings($resource, $class->cid);


    $res = strtolower($resource);
    if ($res == "activeagent") {
      $res = "agent";
    }
    drush_log(dt("Processing @res", array("@res" => $res)));
    if (!$class->override_status_query) {
      //build the query
      $statuses = $class->status_values;
      $status_q = "|$statuses";

      $query = array();
      $query[] = "{$fieldmappings['listing_status']->systemname}={$status_q}";
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
        $listings = array();
        // loop through the search results
        while ($listing = $this->dc->get_phrets()->FetchRow($search)) {
          // calculate the hash
          $listing['hash'] = $this->calculate_hash($listing);
          $listings[] = $listing;
        }
        if ($error = $this->dc->get_phrets()->Error()) {
          drush_log(dt("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
        }
        drush_log(dt("caching @count listings for resource: @resource | class: @class", array("@count" => count($listings), "@resource" => $resource, "@class" => $class->systemname)));
        cache_set("drealty_chunk_{$resource}_{$class->systemname}_" . $chunks++, $listings);

        $offset += count($listings) + 1;
        if ($limit == 'NONE') {
          $end = FALSE;
        } else {
          $end = $this->dc->get_phrets()->IsMaxrowsReached();
        }
        $this->dc->get_phrets()->FreeResult($search);
        // clean up
        //unset($listings, $listing, $search, $optional_params);
        // end the loop if we're getting all the listings in one go
      }
      $this->dc->disconnect();

      // do some cleanup
      unset($listings, $query_fields, $offset, $mls_field, $price_field, $mappings, $resources);

      // at this point we have data waiting to be processed. Need to process the
      // data which will insert/update/delete the listing data as nodes
      drush_log(dt("process_results( connection: @connection_name, resource: @resource, class: @class, chunks: @chunks)", array("@connection_name" => $connection->name, "@resource" => $resource,
            "@class" => $class->systemname, "@chunks" => $chunks)));
      $this->process_results($connection, $resource, $class, $chunks);
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
  protected function process_results(dRealtyConnectionEntity $connection, $resource, $class, $chunk_count) {

    // $first_run = variable_get("drealty_connection_{$connection->conid}_first_run", TRUE);

    $schema = drupal_get_schema_unprocessed("drealty", "drealty_listings");
    $schema_fields = $schema['fields'];

    drush_log('processing results');
    $chunk_idx = 0;
    $in_rets = array();


    $query = new EntityFieldQuery();
    $result = $query->entityCondition('entity_type', 'drealty_listings', '=')
      ->propertyCondition('conid', $connection->conid)
      ->execute();

    $existing_listings_tmp = array();
    if (!empty($result)) {
      $existing_listings_tmp = entity_load('drealty_listings', array_keys($result['drealty_listings']));
    }
    //re-key the array to use the ListingKey 
    $existing_listings = array();
    foreach ($existing_listings_tmp as $existing_listing_tmp) {
      $existing_listings[$existing_listing_tmp->listing_key] = $existing_listing_tmp;
    }

    // get the fieldmappings
    $field_mappings = $connection->FetchFieldMappings($resource, $class->cid);
    // set $id to the systemname of the ListingKey to make the code easier to read
    $id = $field_mappings['listing_key']->systemname;

    for ($i = 0; $i < $chunk_count; $chunk_idx++, $i++) {
      $chunk_name = "drealty_chunk_{$resource}_{$class->systemname}_{$chunk_idx}";
      $rets_listings = cache_get($chunk_name);

      $rets_listings_count = count($rets_listings->data);

      for ($j = 0; $j < $rets_listings_count; $j++) {


        drush_log(dt("Item @idx of @total", array("@idx" => $j + 1, "@total" => $rets_listings_count)));
        $rets_listing = $rets_listings->data[$j];
        $in_rets[] = $rets_listing[$id];

        //TODO: might be a better place for this .. maybe move it into the admin section, not really sure.
//        if ($first_run) {
//
//          drush_log("Running first search query and marking which fields are actually returned in the search as opposed to what the metadata says there is.");
//
//          $first_run_fields = $connection->FetchFields($resource);
//
//          foreach ($first_run_fields as $first_run_field) {
//            if (!isset($rets_listing[$first_run_field->systemname])) {
//              
//              drush_log(dt("Marking @fieldname as not returned", array("@fieldname" => $first_run_field->longname)));
//              
//              db_update('drealty_fields')
//                ->fields(array('rets_returned' => 0))
//                ->condition('systemname', $first_run_field->systemname)
//                ->execute();
//            }
//          }
//          variable_set("drealty_connection_{$connection->conid}_first_run", FALSE);
//        }
        //check to see if this listing is in the db or to see if any of the values have changed
        if (!isset($existing_listings[$rets_listing[$id]]) || $existing_listings[$rets_listing[$id]]->hash != $rets_listing['hash']) {

          $listing = new Entity(array('conid' => $connection->conid), 'drealty_listings');

          // this listing either doesn't exist in the IDX or has changed. 
          // determine if we need to update or create a new one.
          if (isset($existing_listings[$rets_listing[$id]])) {
            // this listing exists so we'll get a reference to it and set the values to what came to us in the RETS result
            $listing = &$existing_listings[$rets_listing[$id]];
          } else {
            $listing->created = time();
          }

          $listing->conid = $connection->conid;
          $listing->name = $rets_listing[$id];
          $listing->class = $class->cid;
          $listing->process_images = TRUE;
          $listing->hash = $rets_listing['hash'];
          $listing->changed = time();
          $listing->rets_imported = TRUE;

          foreach ($field_mappings as $mapping) {
            if (isset($rets_listing[$mapping->systemname])) {

              $value = '';

              switch ($schema_fields[$mapping->field_name]['type']) {
                case 'varchar':
                case 'char':
                  $value = substr($rets_listing[$mapping->systemname], 0, $schema_fields[$mapping->field_name]['length']);
                  break;
                case 'integer':
                case 'float':
                case 'decimal':
                case 'numeric':
                  $val = preg_replace('/[^0-9\.]/Uis', '', $string);
                  $value = is_numeric($val) ? $val : 0;
                  break;
                default:
                  $value = $rets_listing[$mapping->systemname];
              }

              $listing->{$mapping->field_name} = $value;
            }
          }

          if ($class->do_geocoding) {
            $street_number = isset($listing->street_number) ? $listing->street_number : '';
            $street_name = isset($listing->street_name) ? $listing->street_name : '';
            $street_suffix = isset($listing->street_suffix) ? $listing->street_suffix : '';

            $geoaddress = "{$street_number} {$street_name} {$street_suffix}, {$listing->city}, {$listing->state_or_province} {$listing->postal_code}";
            // remove any double spaces
            $geoaddress = str_replace("  ", "", $geoaddress);

            if ($latlon = drealty_geocode($geoaddress)) {
              if ($latlon->success) {
                $listing->latitude = $latlon->lat;
                $listing->longitude = $latlon->lon;
                drush_log(dt('Geocoded: @address to (@lat, @lon)', array('@address' => $geoaddress, '@lat' => $listing->latitude, '@lon' => $listing->longitude)));
              } else {
                drush_log(dt('Failed to Geocode: @address)', array('@address' => $geoaddress)));
              }
            } else {
              drush_log(dt('Failed to Geocode: @address)', array('@address' => $geoaddress)));
            }
          }

          try {
            $listing->save();
          } catch (Exception $e) {
            drush_log($e->getMessage());
          }
          drush_log(dt('Saving listing @name', array('@name' => $listing->name)));
          unset($listing);
        } else {
          // skipping this listing
          drush_log(dt("Skipping listing @name", array("@name" => $rets_listing[$id])));
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
  protected function calculate_hash(array $listing) {
    $tmp = '';
    foreach ($listing as $key => $value) {
      $tmp .= strtolower(trim($value));
    }
    return md5($tmp);
  }

  public function process_images($conid, $resource) {
    $entity_type = 'drealty_listings';
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
              ->entityCondition('entity_type', 'drealty_listings')
              ->propertyCondition('listing_key', $mlskey)
              ->execute();
            $listing = reset(entity_load('drealty_listings', array_keys($result['drealty_listings']), array(), FALSE));

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

