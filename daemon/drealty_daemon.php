<?php
/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
*/

/**
 * Description of drealty_daemon
 *
 * @author chris
 */
class drealty_daemon {


  /**
   *
   * @return <type>
   */
  public function run() {


    module_load_include('php', 'drealty', 'phRets/phRets');

    $connections = drealty_connections_fetch();
    foreach ($connections as $connection) {
      $mappings = drealty_resource_mappings($connection['conid']);
      foreach ($mappings as $mapping) {
        $classes = drealty_classes_fetch($connection['conid'], $mapping['resource']);
        foreach ($classes as $class) {
          if ($class['enabled'] && $class['lifetime'] <= time() - $class['lastupdate'] + 120) {
            $this->process_rets_class($connection['conid'], $mapping['resource'], $class['systemName'], $mapping['node_type']);
            //log the output
            //drush_log(dt("Processing - Connection id: !conid Resource: !resource Class: !class",
            //  array("!conid" => $connection['conid'], "!resource" => $mapping['resource'], "!class" => $class['systemName'])), 'success');
          }
        }
      }
    }

    //clean up
    unset($connections, $mappings, $classes);

    return TRUE;
  }


  protected function process_rets_class($conid, $resource, $class, $type) {

    $query_fields = array();
    $offset = 0;
    $props = array();
    $mls_field = NULL;
    $price_field = NULL;
    $mappings = drealty_resource_mappings($conid);
    $resources = drealty_resources_fetch($conid);

    // build a list of fields we are going to request from the RETS server
    $fields = drealty_fields_active_fetch($conid, $resource);
    $id = '';
    $id_field = '';

    foreach ($fields as $field) {
      $field = (object)$field;
      $field->classes = explode(',', $field->classes);

      if (in_array($class, $field->classes)) {
        $query_fields[] = $field->systemName;
        switch ($type) {
          case 'drealty_property':
            if ($field->correlation === 'mls_id') {
              $mls_field = $field->systemName;
              $id = $field->systemName;
              $id_field = $field->cck_field_name;
            }
            elseif ($field->correlation === 'sale_status') {
              $status_field = $field->systemName;
            }
            elseif ($field->correlation === 'price') {
              $price_field = $field->systemName;
            }
            break;
          case 'drealty_office':
            if ($field->correlation === 'office_id') {
              $office_id = $field->systemName;
              $id = $field->systemName;
              $id_field = $field->cck_field_name;
            }
            elseif ($field->correlation === 'office_type') {
              $status_field = $field->systemName;
            }
            break;
          case 'drealty_agent':
            if ($field->correlation === 'agent_id') {
              $agent_id = $field->systemName;
              $id = $field->systemName;
              $id_field = $field->cck_field_name;
            }
            elseif ($field->correlation === 'agent_type') {
              $status_field = $field->systemName;
            }
            break;
          case 'drealty_open_house':
            if ($field->correlation === 'open_house_id') {
              $open_house_id = $field->systemName;
              $id = $field->systemName;
              $id_field = $field->cck_field_name;
            }
            elseif ($field->correlation === 'sale_status') {
              $status_field = $field->systemName;
            }
            break;
        }
      }
    }

    unset($fields);

    // setup the query
    $statuses = $resources[$mappings[$type]['resource']]['selection_values'];
    $status_q = "|$statuses";

    $query = array();
    $query[] = "$status_field=$status_q";

    if (isset($price_field)) {
      $query[] = "{$price_field}=0+";
    }


    $limit = $resources[$mappings[$type]['resource']]['chunk_size'];
    if ($limit == 0) {
      $limit = 'NONE';
    }
    $chunks = 0;


    if ($this->connect($conid)) {
      // prepare the query
      $q = implode('),(', $query);
      $fields = implode(',', $query_fields);

      $end = TRUE;
      // fetch the search results until we've queried for them all
      while ($end) {
        $end_p = $end?"FALSE":"TRUE";
        drush_log("Resource: $resource Class: $class Limit: $limit Offset: $offset MaxRowsReached: $end_p Chunks: $chunks");
        $optional_params = array(
          'Format' => 'COMPACT-DECODED',
          'Limit' => "$limit",
          'Offset' => "$offset",
          'Select' => "$fields",
          'RestrictedIndicator' => '*****',
          'Count' => '1',
        );
        // do the actual search
        $search = self::get_phrets()->SearchQuery($resource, $class, "($q)", $optional_params);
        $listings = array();
        // loop through the search results
        while ($listing = self::get_phrets()->FetchRow($search)) {
          // calculate the hash
          $listing['crc32'] = $this->calculate_hash($listing);
          $listings[] = $listing;
        }

        cache_set("drealty_chunk_{$type}_{$class}_". $chunks++, $listings);
        $offset += count($listings) + 1;
        $end = self::get_phrets()->IsMaxrowsReached();
        self::get_phrets()->FreeResult($search);
        // clean up
        //unset($listings, $listing, $search, $optional_params);


      }
      $this->disconnect();

      // do some cleanup
      unset($listings, $query_fields, $offset, $mls_field, $price_field, $mappings, $resources);

      // at this point we have data waiting to be processed. Need to process the
      // data which will insert/update/delete the listing data as nodes
      drush_log(dt("process_results(@conid, @resource, @class, @type, @id, @id_field, @chunks)", array("@conid" => $conid, "@resource" => $resource,
        "@class" => $class, "@type" => $type, "@id" => $id, "@id_field" => $id_field, "@chunks" => $chunks)));
      $this->process_results($conid, $resource, $class, $type, $id, $id_field, $chunks);
    }
    else {
      $error = self::get_phrets()->Error();
      watchdog('drealty', "drealty encountered an error: (Type: @type Code: @code Msg: @text)", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), WATCHDOG_ERROR);
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
  protected function process_results($conid, $resource, $class, $type, $id, $id_field, $chunk_count) {

    $db_props = array();
    $result = db_query("SELECT nid, {$type}_crc32_value, dr_{$id_field}_value AS id FROM {content_type_$type} WHERE {$type}_class_value = '%s'", $class);

    while ($prop = db_fetch_object($result)) {
      $db_props[$prop->id] = $prop;
    }

    $chunk_idx = 0;

    $in_rets = array();
    $img_dir = file_directory_path() . '/drealty_img';
    $process_images = variable_get("drealty_use_img_{$resource}_{$conid}", FALSE);

    // grab all the active fields so we can loop through them later
    $active_fields = drealty_fields_active_fetch($conid, $resource);
    $use_loc = variable_get("drealty_use_loc_{$resource}_{$conid}", FALSE);

    for ($i = 0; $i < $chunk_count; $chunk_idx++, $i++) {
      drush_log(dt("Chunk @idx of @total", array("@idx" => $chunk_idx + 1, "@total" => $chunk_count)));
      $chunk_name = "drealty_chunk_{$type}_{$class}_{$chunk_idx}";
      $_props = cache_get($chunk_name);
      $data_count = count($_props->data);

      for ($index = 0; $index < $data_count; $index++) {
        drush_log(dt("Item @idx of @total", array("@idx" => $index + 1, "@total" => $data_count)));
        $prop = (object)$_props->data[$index];
        $in_rets[] = $prop->$id;

        // check to see if this property is in the db or it's hash has changed
        $crc32 = "{$type}_crc32_value";
        if (!isset($db_props[$prop->$id]) || $db_props[$prop->$id]->$crc32 != $prop->crc32) {
          if (isset($db_props[$prop->$id])) {
            $node = node_load($db_props[$prop->$id]->nid,NULL,TRUE);
            $new = FALSE;
          }
          else {
            $node = new stdClass();
            $node->type = $type;
            $node->title = $prop->$id;
            $new = TRUE;
          }
          $location_data = array(
            'street' => array(),
          );

          foreach ($active_fields as $field) {
            $field = (object)$field;

            $system_name = $field->systemName;

            $value = NULL;
            switch ($field->dataType) {
              case 'Date':
                preg_match('/^(\d{4})-(\d{2})-(\d{2})$/', $prop->$system_name, $matches);
                $value = gmmktime(0, 0, 0, $matches[2], $matches[3], $matches[1]);
                break;
              case 'DateTime':
                preg_match('/^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})$/', $prop->$system_name, $matches);
                $value = gmmktime($matches[4], $matches[5], $matches[6], $matches[2], $matches[3], $matches[1]);
                break;
              default:
                if ($field->interpretation === 'LookupMulti') {
                  $value = explode(',', $prop->$system_name);
                }
                else {
                  $value = $prop->$system_name;
                }
            } // end switch $field->dataType

            $field_name = "dr_{$field->cck_field_name}";

            if (is_array($value)) {
              $cck_value = array();
              foreach ($value as $val) {
                $cck_value[] = array('value' => $val);
              }
              $node->{$field_name} = $cck_value;
            }
            else {
              $node->{$field_name} = array(array('value' => $value));
            }

            switch ($field->correlation) {
              case 'body':
                $node->body = $value;
                break;
              case 'pic_count':
                if ($process_images) {
                  drush_log(dt("pic_count: @count", array("@count" => $value)));
                  $node->dr_images = array();
                  $prop_dir = "{$img_dir}";

                  if(!is_dir($prop_dir)) {
                    mkdir($prop_dir);
                  }

                  if (is_dir($prop_dir) ) {

                    if ($this->connect($conid)) {
                      // grab the image data from the RETS server
                      drush_log(dt("Retrieving Image Data for @mls.", array("@mls" => $prop->$id)));
                      $photos = self::get_phrets()->GetObject("Property", "Photo", $prop->$id);
                      // don't need the connection anymore so disconnect
                      $this->disconnect();
                      drush_log(dt("Finished Retrieving Image Data."));

                      // loop through the response from the RETS server
                      foreach ($photos as $photo) {
                        // grab the mlsid and object id from the response. Use these
                        // to name the file.
                        $mlsid = $photo['Content-ID'];
                        $number = $photo['Object-ID'];

                        // make sure we actually retrived this particular image
                        if ($photo['Success'] == TRUE) {
                          // setup the filename and filepath, then write the file to disk
                          $filename = "{$mlsid}-{$number}.jpg";
                          $filepath = "{$prop_dir}/{$filename}";
                          file_put_contents($filepath, $photo['Data'], LOCK_EX);

                          // setup a file array that we can use to update the db
                          $file = array(
                            'uid' => 0,
                            'filename' => $filename,
                            'filepath' => $filepath,
                            'filemime' => $photo['Content-Type'],
                            'status' => FILE_STATUS_PERMANENT,
                            'timestamp' => time(),
                            'filesize' => $photo['Length'],
                          );
                          // insert the file record to the files table
                          drupal_write_record('files', $file, array());

                          // modify this array so we can just reuse it to add to the node
                          $file['title'] = t('dReatly property Image');
                          $file['mimetype'] = $file['filemime'];
                          $file['description'] = '';
                          $file['list'] = TRUE;
                          $file['data'] = array(
                            'alt' => t('property image'),
                            'title' => t('property image'),
                          );
                          // add this image to the node
                          $node->dr_images[] = $file;
                        } // endif $photo['Sucess']
                      } // endfor $photos
                      unset($photos, $file, $filename, $filepath);
                    } // endif $this->connect();
                  } // end if is_dir && mkdir
                }
                break;
              default:
                if (strpos($field->correlation, 'loc_') === 0) {
                  if (preg_match('/^loc_street_(\d+)$/', $field->correlation, $matches)) {
                    if ($value !== '') {
                      $loc['street'][intval($matches[1])] = $value;
                    }
                  }
                  else {
                    $loc[$field->correlation] = $value;
                  }
                }
            } //endswitch $field->correlation
          } // endfor $active_fields

          if ($use_loc) {
            $loc_field_name = "{$type}_loc";
            $node->{$loc_field_name} = array(array());
            foreach (array('postal_code', 'province', 'city') as $item) {
              $loc_field = "loc_{$item}";
              if (isset($loc[$loc_field])) {
                $node->{$loc_field_name}[0][$item] = $loc[$loc_field];
              }
            }
            if (count($loc['street'])) {
              $node->{$loc_field_name}[0]['street'] = implode(' ', $loc['street']);
            }
            $node->{$loc_field_name}[0]['source'] = LOCATION_LATLON_JIT_GEOCODING;
          }

          $class_field_name = "{$type}_class";
          $node->{$class_field_name} = array(
            array(
              'value' => $class,
            ),
          );
          $crc_field_name = "{$type}_crc32";
          $node->{$crc_field_name} = array(
            array(
              'value' => $prop->crc32,
            ),
          );
          $conid_field_name = "{$type}_conid";
          $node->{$conid_field_name} = array(
            array(
              'value' => $conid,
            ),
          );


          $node = node_submit($node);

          if ($new) {
            watchdog('drealty', 'Creating node @nid (@title).', array('@nid' => $node->nid, '@title' => $node->title));
          }
          else {
            watchdog('drealty', 'Updating node @nid (@title).', array('@nid' => $node->nid, '@title' => $node->title));
          }
          $this->drealty_node_save($node);

          drush_log(dt('Creating node @nid (@title).', array('@nid' => $node->nid, '@title' => $node->title)));
          unset($node, $prop, $location_data, $value, $fclone, $_SESSION['messages']);

        } // endif
        else {
          drush_log(dt("Skipping @propid", array("@propid" => $prop->$id)));
        }

      } // endfor $data_count
      unset($_props);
      cache_clear_all($chunk_name, 'cache');
    } // endfor $chunk_count

    cache_clear_all();
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

  /**
   *
   * @param int $conid
   * @return bool
   */
  protected function connect($conid) {

    // check to see if we  are connected to a rets server. If there is a connection
    // do nothing.

    if (!self::$_is_connected) {
      // need to setup a connection
      if (self::get_current_connection()) {
        try {
          return $this->handle_connection();
        }
        catch (drealty_rets_connection_exception $e) {
          watchdog('drealty', $e->getMessage(), NULL, WATCHDOG_ERROR);
          return FALSE;
        }
      }
      else {
        // no connection setup need to create it.
        $connections = drealty_connections_fetch();
        $active_connection = isset($conid)?$connections[$conid]:$default_connection;

        $new_connection = new drealty_connection();

        $new_connection->login_url        = $active_connection['login_url'];
        $new_connection->username         = $active_connection['username'];
        $new_connection->password         = $active_connection['password'];
        $new_connection->force_basic_auth = $active_connection['force_basic_auth'];
        $new_connection->use_compression  = $active_connection['use_compression'];
        $new_connection->ua_string        = $active_connection['ua_string'];
        $new_connection->version          = $active_connection['version'];

        // set the connection

        self::set_current_connection($new_connection);


        $error = FALSE;
        // force basic authentication
        if (self::get_current_connection()->force_basic_auth) {
          self::get_phrets()->SetParam('force_basic_authentication', TRUE);
        }
        // enable compression
        if (self::get_current_connection()->use_compression) {
          self::get_phrets()->SetParam('compression_enabled', TRUE);
        }

        // add some headers
        self::get_phrets()->AddHeader("User-Agent", self::get_current_connection()->ua_string);
        self::get_phrets()->AddHeader("RETS-Version", "RETS/". self::get_current_connection()->version);

        // clean up
        unset($new_connection);
        unset($ative_connection);
        unset($connections);

        try {
          return $this->handle_connection();
        }
        catch (drealty_rets_connection_exception $e) {
          watchdog('drealty', $e->getMessage(), NULL, WATCHDOG_ERROR);
          return FALSE;
        }
      }
    }
  }

  /**
   * Function to perform the actual connection
   * @return bool
   */
  protected function handle_connection() {
    if (!self::get_phrets()->Connect(self::get_current_connection()->login_url, self::get_current_connection()->username, self::get_current_connection()->password)) {
      // we didn't connect, check to see if we have any error results
      if (self::get_phrets()->Error()) {
        // error result present, use that in our exception
        $error_info = self::get_phrets()->Error();
        throw new drealty_rets_connection_exception($error_info['text'], $error_info['code']);
      }
      else {
        // no error result present so we'll throw a generic exception
        throw new drealty_rets_connection_exception('There was an error connecting to the RETS Server.');
      }
    }
    else {
      // connection was successful!
      return TRUE;
    }
  }
  /**
   * Disconnect from the RETS server
   *
   * @return bool
   */
  protected function disconnect() {
    // disconnect if we are connected.
    if (self::get_phrets() && self::$_is_connected) {
      if (self::get_phrets()->Disconnect()) {
        self::$_is_connected = FALSE;
        return TRUE;
      }
      else {
        throw new drealty_rets_disconnect_exception('Error Disconnecting from the RETS Server.');
      }
    }
  }
  function drealty_node_save(&$node) {
    // Let modules modify the node before it is saved to the database.
    node_invoke_nodeapi($node, 'presave');
    global $user;

    // Insert a new node.
    $node->is_new = empty($node->nid);

    if ($node->is_new || !empty($node->revision)) {
      // When inserting a node, $node->log must be set because
      // {node_revisions}.log does not (and cannot) have a default
      // value.  If the user does not have permission to create
      // revisions, however, the form will not contain an element for
      // log so $node->log will be unset at this point.
      if (!isset($node->log)) {
        $node->log = '';
      }
    }
    elseif (empty($node->log)) {
      // When updating a node, however, avoid clobbering an existing
      // log entry with an empty one.
      unset($node->log);
    }

    // For the same reasons, make sure we have $node->teaser and
    // $node->body set.
    if (!isset($node->teaser)) {
      $node->teaser = '';
    }
    if (!isset($node->body)) {
      $node->body = '';
    }

    // Save the old revision if needed.
    if (!$node->is_new && !empty($node->revision) && $node->vid) {
      $node->old_vid = $node->vid;
    }

    $time = time();
    if (empty($node->created)) {
      $node->created = $time;
    }
    // The changed timestamp is always updated for bookkeeping purposes (revisions, searching, ...)
    $node->changed = $time;

    $node->timestamp = $time;
    $node->format = isset($node->format) ? $node->format : FILTER_FORMAT_DEFAULT;

    // Generate the node table query and the node_revisions table query.
    if ($node->is_new) {
      _node_save_revision($node, $user->uid);
      drupal_write_record('node', $node);
      db_query('UPDATE {node_revisions} SET nid = %d WHERE vid = %d', $node->nid, $node->vid);
      $op = 'insert';
    }
    else {
      drupal_write_record('node', $node, 'nid');
      if (!empty($node->revision)) {
        _node_save_revision($node, $user->uid);
        db_query('UPDATE {node} SET vid = %d WHERE nid = %d', $node->vid, $node->nid);
      }
      else {
        _node_save_revision($node, $user->uid, 'vid');
      }
      $op = 'update';
    }


    // Call the node specific callback (if any).
    node_invoke($node, $op);
    module_invoke('content', $op, $node);
    //node_invoke_nodeapi($node, $op);

    // Update the node access table for this node.
    //node_access_acquire_grants($node);

    // Clear the page and block caches.
    //cache_clear_all();
  }

  /**
   * Signleton Design Pattern
   *  @link http://en.wikipedia.org/wiki/Singleton_pattern#PHP
   * @return phRETS
   */
  protected static function get_phrets() {
    if (!self::$_phrets) {
      self::$_phrets = new phRETS();
    }
    return self::$_phrets;
  }
  /**
   * Set the current connection
   *
   * @param drealty_connection $connection
   */
  protected static function set_current_connection(drealty_connection $connection) {
    self::$_current_connection = $connection;
  }
  /**
   * Get a reference to the current connection
   *
   * @return drealty_connection
   */
  protected static function get_current_connection() {
    if (!self::$_current_connection) {
      return NULL;
    }
    return self::$_current_connection;
  }
  // the phRETS class
  protected static $_phrets = NULL;
  // wether we are connected to the RETS server
  protected static $_is_connected = FALSE;
  // the current RETS Connection credentials
  protected static $_current_connection = NULL;

}

class drealty_connection {
  public $name = NULL;
  public $login_url = NULL;
  public $username = NULL;
  public $password = NULL;
  public $ua_string = 'DREALTY/1.0';
  public $version = '1.5';
  public $force_basic_auth = NULL;
  public $use_compression = NULL;
  public $active = NULL;

}

class drealty_no_active_connection_exception extends Exception {

}
class drealty_rets_disconnect_exception extends Exception {

}
class drealty_rets_connection_exception extends Exception {

}