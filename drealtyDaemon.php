<?php

class drealtyDaemon {

  protected $dc;
  protected $dr;
  public $is_drush = FALSE;

  public function __construct() {
    $this->dc = new drealtyConnection();
    $this->dr = new drealtyResources();
  }

  public function runImageProcess($connections_filter = array(), $max = 0, $listing_keys = array(), $chunk_size = 25) {

    $this->log('Image process routine...');
    $this->log('Connections: ' . implode(', ', $connections_filter));
    $this->log('Max: ' . $max);
    $this->log('Listing keys: ' . implode(", ", $listing_keys));

    // Pull all the configured RETS connections from the database and loop through them
    $connections = $this->dc->FetchConnections();
    foreach ($connections as $connection) {

      // This if(){} statement allows the loop to be restricted to a single connection when run
      // from drush with the --connections setting
      if (empty($connections_filter) || in_array((string) $connection->conid, $connections_filter)) {
        $this->log(t("Processing images for connection {$connection->name}, ID {$connection->conid}"));

        // Pull resource mappings (Property, Office, etc) for the connection
        $mappings = $connection->ResourceMappings();

        // Loop through resource mappings
        foreach ($mappings as $mapping) {

          // Fetch classes (Rental, Residential, etc) connected to the mapped resource
          $classes = $connection->FetchClasses($mapping->resource);

          // Loop through classes
          foreach ($classes as $class) {
            // Make sure the class is (a) enabled and (b) was imported more than 
            // X time ago (where X is configured per class in the admin tools)
            if ($class->enabled) {

              // Engage RETS connection to download property images
              $this->process_images($connection, $mapping->resource, $class, $max, $listing_keys, $chunk_size);

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

  public function run($connections_filter = array(), $skip_images = FALSE, $force = FALSE) {

    if ($skip_images) {
      $this->log(t('Skipping images for this import'));
    }
    if ($force) {
      $this->log(t('We will force load results for this import'));
    }

    $connections_to_run = array();

    // Pull all the configured RETS connections from the database and loop through them
    $connections = $this->dc->FetchConnections();

    foreach ($connections as $connection) {
      // This if(){} statement allows the loop to be restricted to a single connection when run
      // from drush with the --connections setting
      if (empty($connections_filter) || in_array((string) $connection->conid, $connections_filter)) {
        $connections_to_run[] = $connection;
        $this->log(t("Including connection {$connection->name}, ID {$connection->conid}"));
      }
      else {
        $this->log(t("Skipping connection {$connection->name}, ID {$connection->conid}"));
      }
    }

    $this->log("\n======================================\n");

    foreach ($connections_to_run as $connection) {
      $this->log(t("Importing for connection {$connection->name}, ID {$connection->conid}"));

      // Pull resource mappings (Property, Office, etc) for the connection
      $mappings = $connection->ResourceMappings();

      // Loop through resource mappings
      foreach ($mappings as $mapping) {

        // Fetch classes (Rental, Residential, etc) connected to the mapped resource
        $classes = $connection->FetchClasses($mapping->resource);

        // Loop through classes
        foreach ($classes as $class) {
          // Make sure the class is (a) enabled and (b) was imported more than 
          // X time ago (where X is configured per class in the admin tools)
          if ($class->enabled && ($force || $class->lifetime <= time() - ($class->lastupdate + 60))) {

            // Engage RETS connection to download properties
            $this->ProcessRetsClass($connection, $mapping->resource, $class, $mapping->entity_type, $skip_images, $force);

            // Update the last updated timestamp to ensure that this class is not
            // re-imported too soon
            $class->lastupdate = time();

            // Write timestamp change to the database
            drupal_write_record('drealty_classes', $class, 'cid');
          }
        }
      }
    }


    unset($connections, $mappings, $classes, $connections_to_run);
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

  private function ProcessRetsClass(dRealtyConnectionEntity $connection, $resource, $class, $entity_type, $skip_images = FALSE, $force = FALSE) {
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
        $error = $this->dc->get_phrets()->Error();
        if ($error) {
          $this->log(t("drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text'])));
          $this->log(t('Error dump: !dump', array('!dump' => print_r($error, TRUE))));
        }

        $this->log(t("caching @count items for resource: @resource | class: @class", array("@count" => count($items), "@resource" => $resource, "@class" => $class->systemname)));

        // Initialize a cache key.  This cache key will be used for storing the 
        // $items array in the Drupal cache for later processing.  Items are stored
        // in "chunks" based on the chunk size (i.e. record limit)
        $cache_key = "drealty_chunk_{$resource}_{$class->systemname}_" . $chunks++;

        // Store the items in the cache
        $cache_result = cache_set($cache_key, $items, 'cache');

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

      // at this point we have data waiting to be processed. Need to process the
      // data which will insert/update/delete the listing data as nodes
      $this->log(t("process_results( connection: @connection_name, resource: @resource, class: @class, chunks: @chunks)", array("@connection_name" => $connection->name, "@resource" => $resource,
                  "@class" => $class->systemname, "@chunks" => $chunks)));

      // Process the results cached from the RETS query/queries
      $this->process_results($connection, $resource, $class, $entity_type, $chunks, $force);

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

    // get the fieldmappings
    $field_mappings = $connection->FetchFieldMappings($resource, $class->cid);

    // set $id to the systemname of the entity's corresponding key from the rets feed to make the code easier to read
    $id = $field_mappings[$key_field]->systemname;


    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', $entity_type, '=')
            ->propertyCondition('conid', $connection->conid);
    if (!empty($rets_item[$id])) {
      $query->propertyCondition($key_field, $rets_item[$id]);
    }
    $result = $query->execute();

    $existing_items_tmp = array();
    if (!empty($result)) {
      $existing_items_tmp = entity_load($entity_type, array_keys($result[$entity_type]));
    }

    $existing_items = array();
    foreach ($existing_items_tmp as $existing_item_tmp) {
      $existing_items[$existing_item_tmp->{$key_field}] = $existing_item_tmp;
    }

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

    return $item;
  }

  /**
   * Insert / Update the listing data
   *
   * @param int $connection
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
   * @param bool $force
   *  If true, force the loading of all results into the database, regardless
    of whether the data has changed.  This will slow down performance.
   */
  protected function process_results(dRealtyConnectionEntity $connection, $resource, $class, $entity_type, $number_of_chunks, $force = FALSE) {

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

    // get the fieldmappings
    $field_mappings = $connection->FetchFieldMappings($resource, $class->cid);

    // set $id to the systemname of the entity's corresponding key from the rets feed to make the code easier to read
    $id = $field_mappings[$key_field]->systemname;

    // Loop through each "chunk" of records stored from the cache
    for ($i = 0; $i < $number_of_chunks; $chunk_idx++, $i++) {

      // Initialize an empty array of listing ids that will be used
      // to look up existing listings in our database
      $ids = array();
      $existing_items_tmp = array();
      $existing_items = array();

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
      for ($j = 0; $j < $rets_results_count; $j++) {
        $rets_item = $rets_results->data[$j];
        $ids[] = $rets_item[$id];
      }

      $query = new EntityFieldQuery();
      $query->entityCondition('entity_type', $entity_type, '=')
              ->propertyCondition('conid', $connection->conid);
      if (!empty($ids)) {
        $query->propertyCondition($key_field, $ids);
      }
      $result = $query->execute();

      // Pull the listing IDs for this RETS connection from the table and put them
      // into $result
      // Load the full entity object for each item in the result set.
      if (!empty($result)) {
        $existing_items_tmp = entity_load($entity_type, array_keys($result[$entity_type]));
      }

      // Put the items into a new associative array based on the appropriate key field.
      foreach ($existing_items_tmp as $existing_item_tmp) {
        $existing_items[$existing_item_tmp->{$key_field}] = $existing_item_tmp;
      }

      // Loop through all records from the cache
      for ($j = 0; $j < $rets_results_count; $j++) {

        $this->log(t("Item @idx of @total", array("@idx" => $j + 1, "@total" => $rets_results_count)));

        // Put the current record into a variable called $rets_item
        $rets_item = $rets_results->data[$j];

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
      else {
        watchdog('drealty_daemon', $message);
      }
    }
  }

  /**
   * 
   * @param dRealtyConnectionEntity $connection
   * @param type $resource
   * @param type $class
   * @param int $max Optional cutoff (max listings to process)
   * @param array $listing_keys 
   * Allows the process_images() method to process select listings by key rather
   * than all listings needing images
   * @return type
   */
  public function process_images(dRealtyConnectionEntity $connection, $resource, $class, $max = 0, $listing_keys = array(), $chunk_size = 25) {

    /**
     * Ideas for optimization:
     * 
     * 1. Don't load all entities at the front.  Load IDs and build chunks
     *    array based on IDs.  For each chunks array, run entity_load and pass
     *    only the group of IDs in the chunk.  This will yield $chunk_size
     *    listing entities, which will consume much less memory than ALL listing
     *    entities at once, assuming that $chunk_size is a reasonable number.
     *    ~~~ DONE ~~~
     * 
     * 2. Unset $items as soon as it has served its purpose
     *    ~~~ DONE ~~~
     * 
     * 3. Don't run the second EntityFieldQuery inside the if ($is_really_an_image) 
     *    block.  This data has been loaded once and just needs to be put into
     *    a lookup structure so it can be referenced again.  No need to consume
     *    those resources when performance is already a big concern.  Right now
     *    this is extremely sloppy and inefficient because the query is run each
     *    time an individual photo is processed.  So if there are 20 photos for 
     *    a listing, we are pulling the listing from the database 20 times.
     *    ~~~ DONE ~~~
     */
    // We have hard-coded the entity type to "drealty_listing"
    $entity_type = 'drealty_listing';

    // Total number of listings processed.  This gets incremented in our
    // loops below
    $total = 0;

    // Pull all the listing records from the database whose process_images
    // flag is set to TRUE.
    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', $entity_type)
            ->propertyCondition('process_images', TRUE)
            ->propertycondition('conid', $connection->conid);

    // If the $listing_keys array was populated with one or more items,
    // restrict the result set to just those items.  This allows us to process
    // images for a small subset of properties if needed.
    if (count($listing_keys) > 0) {
      $this->log('Filtering by listing key');
      $query->propertyCondition('listing_key', $listing_keys);
    }
    if ($max > 0) {
      $query->range(0, $max);
    }
    $result = $query->execute();


    if (!empty($result[$entity_type])) {

      // Put IDs returned from the query into an array
      $result_ids = array_keys($result[$entity_type]);

      $this->log(count($result_ids) . ' listings returned that need images');

      // Split IDs into chunks of $chunk_size
      $process_ids = array_chunk($result_ids, $chunk_size, TRUE);
      // Boolean to indicate whether there are items to process
      $items_exist = count($result_ids > 0);

      // Remove this array from memory
      unset($result_ids);

      //$items = entity_load($entity_type, $result_ids);
    }
    else {
      $this->log("No images to process.");
      return;
    }

    //make sure we have something to process
    if ($items_exist) {
      $this->log("process_images() - Starting.");

      // We don't need $result_ids
      // Set up a base directory for storing images
      $img_dir_base = file_default_scheme() . '://drealty_image';
      $img_dir = $img_dir_base . '/' . $connection->conid;

      // Make sure the directory exists
      file_prepare_directory($img_dir, FILE_MODIFY_PERMISSIONS | FILE_CREATE_DIRECTORY);

      // If we failed to create the directory, quit
      if (!file_prepare_directory($img_dir, FILE_MODIFY_PERMISSIONS | FILE_CREATE_DIRECTORY)) {
        $this->log(t("Failed to create %directory.", array('%directory' => $img_dir)), "error");
        return;
      }
      else {
        // If for some reason the directory still doesn't exist, quit
        if (!is_dir($img_dir)) {
          $this->log(t("Failed to locate %directory.", array('%directory' => $img_dir)), "error");
          return;
        }
      }

      // Split results into chunks of $chunk_size listings at a time
      // $process_array = array_chunk($items, $chunk_size, TRUE);
      // Loop through chunks
      foreach ($process_ids as $ids_chunk) {
        $chunk = entity_load($entity_type, $ids_chunk);
        $ids = array();
        $listings_processed = array();
        $lookup_table = array();


        $external_photos_query = new EntityFieldQuery;
        $external_photos_query->entityCondition('entity_type', 'drealty_external_photo', '=')
                ->propertyCondition('listing_id', $ids_chunk);
        $external_photos_result = $external_photos_query->execute();
        $external_photos_raw = entity_load('drealty_external_photo', array_keys($external_photos_result['drealty_external_photo']));
        $external_photos_lookup_table = array();

        foreach ($external_photos_raw as $p) {
          $external_photos_lookup_table[$p->listing_key][$p->photo_number] = $p;
        }


        // Loop through all items in current chunk and extract the ID
        foreach ($chunk as $item) {
          $ids[] = $item->listing_key;
          $lookup_table[$item->listing_key] = $item;
        }

        // Make sure we have a RETS connection
        if ($this->dc->connect($connection->conid)) {

          // Join the IDs extracted into a comma-separated string to send to the 
          // IDX for querying images
          $id_string = implode(',', array_unique($ids));

          $this->log(t('Fetching object type !type', array('!type' => $class->object_type)));
          $this->log(t('Fetching photos for !count properties', array('!count' => count($ids))));
          $this->log(t('Property IDs are !ids', array('!ids' => implode(', ', $ids))));

          $location = ($class->download_images) ? 1 : 0;

          // Query the IDX for images.  Put the results into $photos
          $photos = $this->dc->get_phrets()->GetObject($resource, $class->object_type, $id_string, '*', $location);

          $this->log(t('!count photos were retrieved from the IDX', array('!count' => count($photos))));

          // If there was an error, log it and quit.
          if ($this->dc->get_phrets()->Error()) {
            $error = $this->dc->get_phrets()->Error();
            $this->log($error['text']);
            return;
          }

          // Close the RETS connection
          $this->dc->disconnect();

          // Eliminate the IDs from memory
          unset($ids);
          unset($id_string);

          // Loop through result set from query
          foreach ($photos as $index => $photo) {
            $mlskey = $photo['Content-ID'];
            $number = $photo['Object-ID'];

            // Get the listing entity object that this photo belongs to
            $listing = $lookup_table[$mlskey];

            if ($class->download_images && $photo['Location']) {
              $this->log(t('Processing external photo !url', array('!url' => $photo['Location'])));
              if ($external_photos_lookup_table[$mlskey][$number]) {
                $external_photo = $external_photos_lookup_table[$mlskey][$number];
              }
              else {
                $external_photo = new Entity(array('conid' => $connection->conid, 'listing_id' => $listing->id, 'listing_key' => $mlskey, 'photo_number' => $number), 'drealty_external_photo');
              }

              $external_photo->photo_url = $photo['Location'];
              $external_photo->description = $photo['Content-Description'];
              $external_photo->save();

              // Remove the process_images flag so that the listing doesn't
              // get included the next time images are downloaded.
              $listing->process_images = 0;

              // Save the listing to the database.
              $listing->save();
            }
            else {

              // Set up destination file name, path, etc.
              $filename = preg_replace('/[^a-zA-Z0-9._-]/', '_', "{$mlskey}-{$number}.jpg");
              $filepath = "{$img_dir}/{$filename}";

              $is_really_an_image = ($photo['Content-Type'] == 'image/jpg' || $photo['Content-Type'] == 'image/png' || $photo['Content-Type'] == 'image/gif' || $photo['Content-Type'] == 'image/jpeg');

              try {

                // Make sure the object retrieved from the IDX is indeed an image
                if ($is_really_an_image) {
                  // See if the file we are trying to save currently exists in the managed
                  // files table
                  $fid = db_query('SELECT fid FROM {file_managed} WHERE filename = :filename', array(':filename' => $filename))->fetchField();

                  // If the file does exist, delete it so we can save the file we
                  // just pulled from the IDX
                  if (!empty($fid)) {
                    $file_object = file_load($fid);
                    file_delete($file_object, TRUE);
                  }

                  $this->log(t("Saving @filename", array("@filename" => $filepath)));

                  // Save the photo to the filesystem
                  $file = file_save_data($photo['Data'], $filepath, FILE_EXISTS_REPLACE);


                  if (!array_key_exists($mlskey, $listings_processed)) {
                    $listings_processed[$mlskey] = 0;
                  }
                  $listings_processed[$mlskey]++;

                  // Remove the process_images flag so that the listing doesn't
                  // get included the next time images are downloaded.
                  $listing->process_images = 0;

                  // Save the listing to the database.
                  $listing->save();


                  // Map the photo to the listing.
                  file_usage_add($file, 'drealty', $entity_type, $listing->id);
                }
                else {
                  $this->log(t('Photo !num is not an image. Content type is !type', array('!num' => $index, '!type' => $photo['Content-Type'])));
                  if ($photo['Content-Type'] == 'text/xml') {
                    $this->log($photo['Data']);
                    // Evidently this listing doesn't have photos, so don't try to
                    // pull photos again
                    $listing->process_images = 0;
                    $listing->save();
                  }
                }
              } catch (Exception $ex) {
                $this->log(t('EXCEPTION SAVING FILE: !ex', array('!ex' => $ex->getMessage())));
                $this->log(t('MLS Key: !m', array('!m' => $mlskey)));
                $this->log(t('Connection ID: !m', array('!m' => $connection->conid)));
              }
            }
          }

          // Increment total by the number of unique listings that were processed
          // in the batch
          $total += count(array_keys($listings_processed));
          $this->log(t('The total number of listings processed so far is !num', array('!num' => $total)));

          // Remove the downloaded photos from memory
          unset($photos);
        }

        unset($chunk);
        unset($lookup_table);

        /**
         * $max is a built-in cutoff that can force this function to break out of
         * the loop when a certain threshold is hit.  This check is done outside of 
         * individual photos in a chunk, so $max will not be respected exactly.
         * The possible variance will be $max + $chunk_size.
         */
        if ($max > 0 && $max <= $total) {
          $this->log(t("Breaking because we hit !total of !max maximum records.", array('!total' => $total, '!max' => $max)));
          break;
        }
      }
    }
  }

}

