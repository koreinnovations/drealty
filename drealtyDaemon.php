<?php

class drealtyDaemon {

  protected $dc;
  protected $dr;
  public $is_drush = FALSE;

  public function __construct() {
    $this->dc = new drealtyConnection();
    $this->dr = new drealtyResources();
  }

  public function run($connections_filter = array()) {

    // Pull all the configured RETS connections from the database and loop through them
    $connections = $this->dc->FetchConnections();
    foreach ($connections as $connection) {

      // This if(){} statement allows the loop to be restricted to a single connection when run
      // from drush with the --connections setting
      if (empty($connections_filter) || in_array((string) $connection->conid, $connections_filter)) {
        $this->log(t("Importing for connection {$connection->name}, ID {$connection->conid}"));

        // Pull resource mappings (Property, Office, etc) for the connection
        $mappings = $connection->ResourceMappings();

        $this->log(t('Resource mappings !dump', array('!dump' => print_r($mappings, TRUE))));

        // Loop through resource mappings
        foreach ($mappings as $mapping) {

          // Fetch classes (Rental, Residential, etc) connected to the mapped resource
          $classes = $connection->FetchClasses($mapping->resource);

          $this->log(t('Classes !dump', array('!dump' => print_r($classes, TRUE))));

          // Loop through classes
          foreach ($classes as $class) {
            // Make sure the class is (a) enabled and (b) was imported more than 
            // X time ago (where X is configured per class in the admin tools)
            if ($class->enabled && $class->lifetime <= time() - ($class->lastupdate + 60)) {

              // Engage RETS connection to download properties
              $this->ProcessRetsClass($connection, $mapping->resource, $class, $mapping->entity_type);

              // Update the last updated timestamp to ensure that this class is not
              // re-imported too soon
              $class->lastupdate = time();

              // Write timestamp change to the database
              drupal_write_record('drealty_classes', $class, 'cid');
            }
          }
        }
      }
      else {
        $this->log(t("Skipping connection {$connection->name}, ID {$connection->conid}"));
      }
    }
    unset($connections, $mappings, $classes);
    cache_clear_all();
    return TRUE;
  }

  function perform_query(drealtyConnectionEntity $connection, $resource, $class, $query) {
    $items = array();
    $rets = $this->dc->get_phrets();
    $limit = $class->chunk_size;
    if ($limit == 0) {
      $limit = 'NONE';
    }
    $count = 0;

    //$this->dc->get_phrets()->SetParam("offset_support", TRUE);

    if ($this->dc->connect($connection->conid)) {
      $optional_params = array(
          'Format' => 'COMPACT-DECODED',
          'Limit' => "$limit",
          'Offset' => "$offset",
      );

      // do the actual search
      $search = $rets->SearchQuery($resource, $class->systemname, $query, $optional_params);

      // loop through the search results
      while ($listing = $rets->FetchRow($search)) {
        if ($count < 10) {
          $listing['hash'] = $this->calculate_hash($listing);
          $items[] = $listing;
        }
        $count++;
      }
      if ($error = $this->dc->get_phrets()->Error()) {
        dpm(t("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text'])));

        dpm(t('Error dump: !dump', array('!dump' => print_r($error, TRUE))));
      }

      $rets->FreeResult($search);

      $this->dc->disconnect();

      return $items;
    }
    else {
      $error = $rets->Error();
      watchdog('drealty', "drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), WATCHDOG_ERROR);
    }
  }

  private function ProcessRetsClass(dRealtyConnectionEntity $connection, $resource, $class, $entity_type) {
    $query_fields = array();
    $offset = 0;
    $props = array();
    $mls_field = NULL;
    $price_field = NULL;

    // Pull resources for the connection
    // Resources are the "object types" from the RETS connection metadata
    $resources = $connection->Resources();

    // Pull resource mappings for the connection
    // These mappings connect the resource objects from the RETS connection
    // to Drupal entities
    $mappings = $connection->ResourceMappings();

    // build a list of fields we are going to request from the RETS server
    $fieldmappings = $connection->FetchFieldMappings($resource, $class->cid);

    $this->log(t("Processing @res", array("@res" => $resource)));

    // In the class configuration screen of the admin tools, a custom RETS
    // query can be specified to override the one provided by the framework.
    // If this option has been selected, set the query to the contents of the
    // custom query text field from the admin tools.
    if ($class->override_status_query) {
      $query = array();
      $this->log(t("using @var", array("@var" => $class->override_status_query_text)));
      $query[] = $class->override_status_query_text;
    }
    // We are not using a custom query, so go ahead and build a query based on
    // listing statuses or type that the user wants to filter on.
    else {
      //build the query
      $statuses = $class->status_values;
      $status_q = "|$statuses";

      switch ($entity_type) {
        // If the resource is a listing or an open house, filter on "listing status"
        case 'drealty_listing':
        case 'drealty_openhouse':
          $query_field = 'listing_status';
          break;
        // If the resource is an agent or an office, filter on "type"
        case 'drealty_agent':
        case 'drealty_office':
          $query_field = 'type';
          break;
        // By default, filter on "listing status"
        default:
          $query_field = 'listing_status';
      }

      $query = array();
      $query[] = "{$fieldmappings[$query_field]->systemname}={$status_q}";
    }


    // Pull the limit of records to process in one batch.  This is set in the 
    // admin tools
    $limit = $class->chunk_size;

    // If the limit is 0, set it to the string value 'NONE'
    if ($limit == 0) {
      $limit = 'NONE';
    }

    // Chunks is used to break listings into batches for faster processing
    // and/or better memory management.  This value will be incremented as
    // we iterate through the listings retrieved from the RETS query
    $chunks = 0;

    // Make sure we get a successful connection with the IDX
    if ($this->dc->connect($connection->conid)) {

      // Convert the query from an array to a string, with each element segregated
      // by parentheses
      $q = implode('),(', $query);
      // $fields = implode(',', $query_fields);
      // TODO: Find out what this does
      $keep_going = TRUE;

      // fetch the search results until we've queried for them all
      while ($keep_going) {

        $end_p = $keep_going ? "FALSE" : "TRUE";

        $this->log("Resource: $resource Class: $class->systemname Limit: $limit Offset: $offset MaxRowsReached: $end_p Chunks: $chunks");

        /*
         * Configure parameters to be sent with the RETS query.
         * 
         * - Format
         *   Determines how the results are returned.  Default is COMPACT-DECODED, 
         *   but can be adjusted from the admin tools
         * 
         * - Limit
         *   The maximum number of records to return at once
         * 
         * - Offset
         *   The starting record offset.  Used for pulling results in batches.
         *   Starts out at 0 and gets increased with each iteration through
         *   this loop
         * 
         * - RestrictedIndicator
         *   Not sure. Somebody needs to document this
         * 
         * - Count
         *   Not sure. Somebody needs to document this
         */
        $optional_params = array(
            'Format' => ($class->format) ? $class->format : 'COMPACT-DECODED',
            'Limit' => "$limit",
            'Offset' => "$offset",
            'RestrictedIndicator' => 'xxxx',
            'Count' => '1',
        );


        $this->log(t('Class dump: !dump', array('!dump' => print_r($class, TRUE))));
        $this->log(t('Query dump: !dump', array('!dump' => print_r("($q)", TRUE))));
        $this->log(t('Params dump: !dump', array('!dump' => print_r($optional_params, TRUE))));

        // Perform the RETS query against the IDX server
        $search = $this->dc->get_phrets()->SearchQuery($resource, $class->systemname, "($q)", $optional_params);

        $this->log(t("Rows returned: " . $this->dc->get_phrets()->TotalRecordsFound()));

        // Initialize an empty array to store items returned from the IDX server
        $items = array();

        // loop through the search results
        while ($item = $this->dc->get_phrets()->FetchRow($search)) {

          // Calculate a hash based on the unique data in this result record.
          // This is used to determine if the data has changed since it was last
          // stored in our system
          $item['hash'] = $this->calculate_hash($item);

          // Add $item to the array of items returned.
          $items[] = $item;
        }

        // Catch any errors that occurred
        if ($error = $this->dc->get_phrets()->Error()) {
          $this->log(t("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text'])));
          $this->log(t('Error dump: !dump', array('!dump' => print_r($error, TRUE))));
        }

        $this->log(t("caching @count items for resource: @resource | class: @class", array("@count" => count($items), "@resource" => $resource, "@class" => $class->systemname)));

        // Initialize a cache key.  This cache key will be used for storing the 
        // $items array in the Drupal cache for later processing.  Items are stored
        // in "chunks" based on the chunk size (i.e. record limit)
        $cache_key = "drealty_chunk_{$resource}_{$class->systemname}_" . $chunks++;

        $this->log($cache_key);

        // Store the items in the cache
        $cache_result = cache_set($cache_key, $items, 'cache');

        $this->log(t('Cache result: !res', array('!res' => print_r($cache_result, TRUE))));

        // Increment offset so that our next RETS query will start where we left
        // off and we won't pull duplicate records.
        $offset += count($items) + 1;

        // If no limit was set, don't go back into the loop. 
        if ($limit == 'NONE') {
          $keep_going = FALSE;
        }
        else {
          // If our RETS query reached the maximum rows allowed, keep going
          // and run another query
          $keep_going = $this->dc->get_phrets()->IsMaxrowsReached();
        }

        // Free the current result set from memory
        $this->dc->get_phrets()->FreeResult($search);
      }

      // Disconnect the open RETS connection
      $this->dc->disconnect();

      // do some cleanup, get the following items out of memory
      unset($items, $query_fields, $offset, $mls_field, $price_field, $mappings, $resources);

      $skip_images = FALSE;

      // at this point we have data waiting to be processed. Need to process the
      // data which will insert/update/delete the listing data as nodes
      $this->log(t("process_results( connection: @connection_name, resource: @resource, class: @class, chunks: @chunks)", array("@connection_name" => $connection->name, "@resource" => $resource,
                  "@class" => $class->systemname, "@chunks" => $chunks)));

      // Process the results cached from the RETS query/queries
      $this->process_results($connection, $resource, $class, $entity_type, $chunks);

      // Process any images associated with the items downlaoded
      if ($entity_type == 'drealty_listing' && $class->process_images && !$skip_images) {
        $this->process_images($connection, $resource, $class);
      }
    }
    else {
      $error = $this->dc->get_phrets()->Error();
      watchdog('drealty', "drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), WATCHDOG_ERROR);
      $this->log(t("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), 'error'));
    }
  }

  public function process_result(dRealtyConnectionEntity $connection, $resource, $class, $rets_item) {
    $schema = drupal_get_schema_unprocessed("drealty", $entity_type);
    $schema_fields = $schema['fields'];
    $entity_type = 'drealty_listing';
    $key_field = 'listing_key';

    $query = new EntityFieldQuery();
    $result = $query->entityCondition('entity_type', $entity_type, '=')
            ->propertyCondition('conid', $connection->conid)
            ->execute();

    $existing_items_tmp = array();
    if (!empty($result)) {
      $existing_items_tmp = entity_load($entity_type, array_keys($result[$entity_type]));
    }

    $existing_items = array();
    foreach ($existing_items_tmp as $existing_item_tmp) {
      $existing_items[$existing_item_tmp->{$key_field}] = $existing_item_tmp;
    }

    // get the fieldmappings
    $field_mappings = $connection->FetchFieldMappings($resource, $class->cid);

    // set $id to the systemname of the entity's corresponding key from the rets feed to make the code easier to read
    $id = $field_mappings[$key_field]->systemname;

    $item = new Entity(array('conid' => $connection->conid), $entity_type);

    // this listing either doesn't exist in the IDX or has changed. 
    // determine if we need to update or create a new one.
    if (isset($existing_items[$rets_item[$id]])) {
      // this listing exists so we'll get a reference to it and set the values to what came to us in the RETS result
      $item = &$existing_items[$rets_item[$id]];
    }
    else {
      $item->created = time();
    }

    $item->conid = $connection->conid;
    $item->name = $rets_item[$id];
    $item->hash = $rets_item['hash'];
    $item->changed = time();
    $item->class = $class->cid;
    $item->rets_imported = TRUE;

    if ($entity_type == 'drealty_listing') {
      $item->process_images = TRUE;
      $item->download_images = $class->download_images;
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
            if (preg_match('/date/', $mapping->field_name)) {
              $this->log($string);
              $value = strtotime($string);
              break;
            }
            else {
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

    $item->raw_mls_data = json_encode($rets_item);

    if ($class->do_geocoding) {
      $street_number = isset($item->street_number) ? $item->street_number : '';
      $street_name = isset($item->street_name) ? $item->street_name : '';
      $street_suffix = isset($item->street_suffix) ? $item->street_suffix : '';

      $street = trim("{$street_number} {$street_name} {$street_suffix}");

      $geoaddress = "{$street}, {$item->city}, {$item->state_or_province} {$item->postal_code}";
      // remove any double spaces
      $geoaddress = str_replace("  ", "", $geoaddress);

      if ($latlon = drealty_geocode($geoaddress)) {
        if ($latlon->success) {
          $item->latitude = $latlon->lat;
          $item->longitude = $latlon->lon;
        }
      }
    }

    try {
      $item->save();
    } catch (Exception $e) {
      
    }
    unset($item);
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
   * @param int $number_of_chunks
   *  The number of chunks that need to be processed
   *
   */
  protected function process_results(dRealtyConnectionEntity $connection, $resource, $class, $entity_type, $number_of_chunks) {

    // $first_run = variable_get("drealty_connection_{$connection->conid}_first_run", TRUE);
    // Pull the schema for the appropriate drealty table
    $schema = drupal_get_schema_unprocessed("drealty", $entity_type);
    // Pull the field definitions from the table
    $schema_fields = $schema['fields'];

    // Determine which field to use as the listing "key"
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

    $this->log('processing results');


    $chunk_idx = 0;

    // Pull the listing IDs for this RETS connection from the table and put them
    // into $result
    $query = new EntityFieldQuery();
    $result = $query->entityCondition('entity_type', $entity_type, '=')
            ->propertyCondition('conid', $connection->conid)
            ->execute();

    // Load the full entity object for each item in the result set.
    // 
    // TODO: why are we doing this?????
    // IDEA: Load in results per chunk using entity_load() and pass IDs of all entities in active chunk
    $existing_items_tmp = array();
    if (!empty($result)) {
      $existing_items_tmp = entity_load($entity_type, array_keys($result[$entity_type]));
    }

    // Put the items into a new associative array based on the appropriate key field.
    $existing_items = array();
    foreach ($existing_items_tmp as $existing_item_tmp) {
      $existing_items[$existing_item_tmp->{$key_field}] = $existing_item_tmp;
    }

    // get the fieldmappings
    $field_mappings = $connection->FetchFieldMappings($resource, $class->cid);

    // set $id to the systemname of the entity's corresponding key from the rets feed to make the code easier to read
    $id = $field_mappings[$key_field]->systemname;

    // Loop through each "chunk" of records stored from the cache
    for ($i = 0; $i < $number_of_chunks; $chunk_idx++, $i++) {

      // Identify the name of the "cache key", which is a composite of the 
      // following variables:
      $chunk_name = "drealty_chunk_{$resource}_{$class->systemname}_{$chunk_idx}";

      $this->log($chunk_name);

      // Pull in the cached data
      $rets_results = cache_get($chunk_name);
      // Pull in the number of records in the cached cunk
      $rets_results_count = count($rets_results->data);
      
      // Initially loop through all records from the cache and pull the record 
      // IDs so we can query our database to find listings
//      for ($j = 0; $j < $rets_results_count; $j++) {
//        $rets_item = $rets_results->data[$j];
//        $lookup_key = $rets_item[$id];
//      }

      // Loop through all records from the cache
      for ($j = 0; $j < $rets_results_count; $j++) {

        $this->log(t("Item @idx of @total", array("@idx" => $j + 1, "@total" => $rets_results_count)));

        // Put the current record into a variable called $rets_item
        $rets_item = $rets_results->data[$j];


        $this->log(t('Raw item dump: !dump', array('!dump' => print_r($rets_item, TRUE))));


        // If true, force the loading of all results into the database, regardless
        // of whether the data has changed.  This will slow down performance.
        $force = FALSE;
        
        // Only process images from the IDX if we're not in "force" mode.
        $process_images_from_rets = !$force;
        // Only attempt geocoding of properties if this feature is turned on from
        // the admin tools AND we're not in force mode
        $attempt_geocoding = $class->do_geocoding && !$force;
        // Does the listing already exist in the database?
        $listing_already_exists = isset($existing_items[$rets_item[$id]]);

        /**
         * A TRUE condition for any of the following checks will cause the data 
         * from the cache to be inserted/updated into the database
         * 
         * 1. The item does NOT currently exist in the database
         * 
         * 2. The item from the cache exists in the database, but has changed. 
         *    (This can be detected by comparing the hashes of the two items.
         *    If the hashes are the same, the data has not changed.)
         * 
         * 3. The $force flag is set to TRUE.  This can be used during development
         *    or anytime the developer needs to make sure that all records are 
         *    processed or re-processed in the database
         */
        if (!$listing_already_exists || $existing_items[$rets_item[$id]]->hash != $rets_item['hash'] || $force) {

          // Create an instance of the entity type
          $item = new Entity(array('conid' => $connection->conid), $entity_type);

          // If the listing already exists, set $item to the existing entity object
          if ($listing_already_exists) {
            // this listing exists so we'll get a reference to it and set the values to what came to us in the RETS result
            $item = &$existing_items[$rets_item[$id]];
          }
          // Otherwise, this will be a new listing...
          else {
            $item->created = time();
          }

          $item->conid = $connection->conid;
          $item->name = $rets_item[$id];
          $item->hash = $rets_item['hash'];
          $item->changed = time();
          $item->class = $class->cid;
          $item->rets_imported = TRUE;

          if ($entity_type == 'drealty_listing') {
            $item->process_images = $process_images_from_rets;
            $item->download_images = $class->download_images;
          }

          // Loop through the RETS data to Entity field mappings array and assign
          // data from the cached RETS result object to the appropriate field on
          // the entity object
          foreach ($field_mappings as $mapping) {
            if (isset($rets_item[$mapping->systemname])) {

              $value = '';

              // The value will be handled differently depending on the data
              // type of the field in our database
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
                  if (preg_match('/date/', $mapping->field_name)) {
                    $this->log($string);
                    $value = strtotime($string);
                    break;
                  }
                  else {
                    $val = preg_replace('/[^0-9\.]/Uis', '', $string);
                    $value = is_numeric($val) ? $val : 0;
                  }
//                  switch ($mapping->field_name) {
//                    case 'end_datetime':
//                    case 'start_datetime':
//                      $this->log($string);
//                      $value = strtotime($string);
//                      break;
//                    default:
//                      $val = preg_replace('/[^0-9\.]/Uis', '', $string);
//                      $value = is_numeric($val) ? $val : 0;
//                  }
                  break;
                default:
                  $value = $rets_item[$mapping->systemname];
              }
              $item->{$mapping->field_name} = $value;
            }
          }

          // Store a copy of the raw MLS data into the database record.  This 
          // is a JSON encoding of the original RETS result object.
          $item->raw_mls_data = json_encode($rets_item);

          // If appropriate, go ahead and try to geocode the record
          if ($attempt_geocoding) {

            // TODO: This logic may need to be cleaned up a little bit later.
            // Some MLS vendors configure their addresses in a weird way, where
            // the city, state and zip are part of the "street".  Still, others
            // put everything in an "unparsed address" field that may be good to
            // query.

            $street_number = isset($item->street_number) ? $item->street_number : '';
            $street_name = isset($item->street_name) ? $item->street_name : '';
            $street_suffix = isset($item->street_suffix) ? $item->street_suffix : '';

            $street = trim("{$street_number} {$street_name} {$street_suffix}");

            $geoaddress = "{$street}, {$item->city}, {$item->state_or_province} {$item->postal_code}";
            // remove any double spaces
            $geoaddress = str_replace("  ", "", $geoaddress);

            $latlon = drealty_geocode($geoaddress);
            if ($latlon && $latlon->success) {
              $item->latitude = $latlon->lat;
              $item->longitude = $latlon->lon;
              $this->log(t('Geocoded: @address to (@lat, @lon)', array('@address' => $geoaddress, '@lat' => $item->latitude, '@lon' => $item->longitude)));
            }
            else {
              $this->log(t('Failed to Geocode: @address', array('@address' => $geoaddress)));
            }
          }

          // Save the item to the database
          try {
            $item->save();
          } catch (Exception $e) {
            $this->log($e->getMessage());
          }

          $this->log(t('Saving item @name', array('@name' => $item->name)));
          // Remove the item from memory
          unset($item);
        }
        else {
          // skipping this item
          $this->log(t("Skipping item @name", array("@name" => $rets_item[$id])));
        }
      }
      //cache_clear_all($chunk_name, 'cache');
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

  private function log($message) {
    if ($this->is_drush) {
      drush_log($message);
    }
    else {
      if (module_exists('devel')) {
        dpm($message);
      }
    }
  }

  public function process_images($conid, $resource, $class, $max = 0) {
    $entity_type = 'drealty_listing';
    $chunk_size = 25;
    $total = 0;

    $query = new EntityFieldQuery();
    $result = $query
            ->entityCondition('entity_type', $entity_type)
            ->propertyCondition('process_images', TRUE)
            ->propertycondition('conid', $conid->conid)
            ->execute();

    if (!empty($result[$entity_type])) {
      $items = entity_load($entity_type, array_keys($result[$entity_type]));
    }
    else {
      $this->log("No images to process.");
      return;
    }

    //make sure we have something to process
    if (count($items) >= 1) {
      $this->log("process_images() - Starting.");
      $img_dir_base = file_default_scheme() . '://drealty_image';
      $img_dir = $img_dir_base . '/' . $conid->conid;

      file_prepare_directory($img_dir, FILE_MODIFY_PERMISSIONS | FILE_CREATE_DIRECTORY);

      if (!file_prepare_directory($img_dir, FILE_MODIFY_PERMISSIONS | FILE_CREATE_DIRECTORY)) {
        $this->log(t("Failed to create %directory.", array('%directory' => $img_dir)), "error");
        return;
      }
      else {
        if (!is_dir($img_dir)) {
          $this->log(t("Failed to locate %directory.", array('%directory' => $img_dir)), "error");
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
          $this->log("id string: " . $id_string);

          $photos = $this->dc->get_phrets()->GetObject($resource, $class->object_type, $id_string, '*');

          if ($this->dc->get_phrets()->Error()) {
            $error = $this->dc->get_phrets()->Error();
            $this->log($error['text']);
            return;
          }

          $this->dc->disconnect();

          unset($ids);
          $id_string = "";

          foreach ($photos as $photo) {
            $this->log($photo);
            $mlskey = $photo['Content-ID'];
            $number = $photo['Object-ID'];
            $filename = preg_replace('/[^a-zA-Z0-9._-]/', '_', "{$mlskey}-{$number}.jpg");
            $filepath = "{$img_dir}/{$filename}";


            $fid = db_query('SELECT fid FROM {file_managed} WHERE filename = :filename', array(':filename' => $filename))->fetchField();

            if (!empty($fid)) {
              $file_object = file_load($fid);
              file_delete($file_object, TRUE);
            }

            $this->log(t("Saving @filename", array("@filename" => $filepath)));

            try {

              $log = print_r(array(
                  'Content-Type' => $photo['Content-Type'],
                  'Success' => $photo['Success'],
                  'Object-ID' => $photo['Object-ID'],
                  'Content-ID' => $photo['Content-ID'],
                      ), TRUE);

              $this->log(t('Photo result: !dump', array('!dump' => $log)));

              if ($photo['Content-Type'] == 'image/jpg' || $photo['Content-Type'] == 'image/png' || $photo['Content-Type'] == 'image/gif' || $photo['Content-Type'] == 'image/jpeg') {
                $file = file_save_data($photo['Data'], $filepath, FILE_EXISTS_REPLACE);
                // load the entity that is associated with the image
                $query = new EntityFieldQuery();

                $result = $query
                        ->entityCondition('entity_type', 'drealty_listing')
                        ->propertyCondition('listing_key', $mlskey)
                        ->propertyCondition('conid', $conid->conid)
                        ->execute();


                $listing = reset(entity_load('drealty_listing', array_keys($result['drealty_listing']), array(), FALSE));

                file_usage_add($file, 'drealty', $entity_type, $listing->id);

                $listing->process_images = 0;
                $listing->save();
              }
            } catch (Exception $ex) {
              $this->log(t('EXCEPTION SAVING FILE: !ex', array('!ex' => $ex->getMessage())));
              $this->log(t('MLS Key: !m', array('!m' => $mlskey)));
              $this->log(t('Connection ID: !m', array('!m' => $conid->conid)));
              //$this->log(t('Photo: !m', array('!m' => print_r($photo, TRUE))));
            }

            $total++;
          }
          unset($photos);
        }

        if ($max > 0 && $total >= $max)
          break;
      }
    }
    cache_clear_all("prop_images_to_process", "cache");
  }

}

