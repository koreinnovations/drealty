<?php
// $Id$


/**
 * @file drealty.batch.inc
 */



/**
 * Define the batch jobs.
 *
 * Unless the batch has been marked with 'progressive' = FALSE, the function issues a drupal_goto and thus ends page execution.
 *
 * @param int $conid
 *  Connection ID. Id for the connection to operate on.
 * @param string $resource
 *  The Resource Name. The SystemName of the resource to operate on.
 * @param string $class
 *  The Class Name. The SystemName of the class to operate on.
 * @param bool $progressive
 *  Batch API. Wether to set the batch to Progressive or Non-Progressive.
 *
 * @see http://api.drupal.org/api/function/batch_process/6.
 *
 * @return none
 *
 */
function drealty_import_batch($conid, $resource, $class, $type, $progressive = TRUE) {
  $classes = drealty_classes_fetch($conid, $resource);
  if (isset($classes[$class]) && $classes[$class]['enabled']) {
    $batch = array(
      'title' => t('Updating @resource in class @class (@visible)', array('@resource' => $resource, '@class' => $class, '@visible' => $classes[$class]['standardName'])),
      'operations' => array(
        array('drealty_import_batch_fetch', array($conid, $resource, $class, $type)),
        array('drealty_import_batch_update', array()),
        array('drealty_import_batch_del', array()),
      ),
      'finished' => 'drealty_import_batch_finished',
      'file' => './' . drupal_get_path('module', 'drealty') . '/drealty.batch.inc',
    );
    batch_set($batch);
    // http://drupal.org/node/638712
    $batch =& batch_get();
    $batch['progressive'] = $progressive;
    batch_process("admin/drealty/connection/manual_update/{$conid}");
  }
  else {
    drupal_not_found();
    return;
  }
}


/**
 *
 * Fetch the listings from the RETS server. Batch Callback.
 *
 * @param int $conid
 *  The connection id.
 * @param string $resource
 *  The RETS resource name.
 * @param string $class
 *  The RETS Class Name
 * @param string $type
 *  The node type this resourc/class is associated to
 * @param array() $context
 *  Batch API Context
 *
 */
function drealty_import_batch_fetch($conid, $resource, $class, $type, &$context) {
  // setup if this is the first time running
  if (!count($context['sandbox'])) {
    $context['sandbox']['offset'] = 0;
    $context['sandbox']['props'] = array();
    $context['results']['ask_fields'] = array();
    $context['results']['conid'] = $conid;
    $context['results']['resource'] = $resource;
    $context['results']['class'] = $class;
    $context['results']['type'] = $type;
    $context['results']['error'] = FALSE;
    $context['results']['counts'] = array(
      'checked' => 0,
      'new' => 0,
      'del' => 0,
      'keep' => 0,
    );
    $price_field = NULL;
    $mappings = drealty_resource_mappings($conid);
    $resources = drealty_resources_fetch($conid);

    //build a list of fields we are going to request from the RETS server
    $context['results']['fields'] = drealty_fields_active_fetch($conid, $resource);

    //$context_fields = &$context['results']['fields'];

    foreach ($context['results']['fields'] as $field) {
      $context['results']['fields'][$field['systemName']]['classes'] = explode(',', $context['results']['fields'][$field['systemName']]['classes']);
      if (in_array($class, $context['results']['fields'][$field['systemName']]['classes'])) {
        $context['results']['ask_fields'][] = $field['systemName'];

        // do type specific query setup
        switch ($type) {
          case 'drealty_property':
            if ($context['results']['fields'][$field['systemName']]['correlation'] === 'mls_id') {
              $context['results']['mls_field'] = $context['results']['fields'][$field['systemName']]['systemName'];
            }
            elseif ($context['results']['fields'][$field['systemName']]['correlation'] === 'sale_status') {
              $status_field = $context['results']['fields'][$field['systemName']]['systemName'];
            }
            elseif ($context['results']['fields'][$field['systemName']]['correlation'] === 'price') {
              $price_field = $context['results']['fields'][$field['systemName']]['systemName'];
            }
            break;
          case 'drealty_office':
            if ($context['results']['fields'][$field['systemName']]['correlation'] === 'office_id') {
              $context['results']['office_id'] = $context['results']['fields'][$field['systemName']]['systemName'];
            }
            elseif ($context['results']['fields'][$field['systemName']]['correlation'] === 'office_type') {
              $status_field = $context['results']['fields'][$field['systemName']]['systemName'];
            }
            break;
          case 'drealty_agent':
            if ($context['results']['fields'][$field['systemName']]['correlation'] === 'agent_id') {
              $context['results']['agent_id'] = $context['results']['fields'][$field['systemName']]['systemName'];
            }
            elseif ($context['results']['fields'][$field['systemName']]['correlation'] === 'agent_type') {
              $status_field = $context['results']['fields'][$field['systemName']]['systemName'];
            }
            break;
          case 'drealty_open_house':
            if ($context['results']['fields'][$field['systemName']]['correlation'] === 'open_house_id') {
              $context['results']['open_house_id'] = $context['results']['fields'][$field['systemName']]['systemName'];
            }
            elseif ($context['results']['fields'][$field['systemName']]['correlation'] === 'sale_status') {
              $status_field = $context['results']['fields'][$field['systemName']]['systemName'];
            }
            break;
        }
      }
    }

    //build the query
    $statuses = $resources[$mappings[$type]['resource']]['selection_values'];
    $status_q = "|$statuses";

    $context['sandbox']['query'] = array();
    $context['sandbox']['query'][] = "{$status_field}={$status_q}";

    if (isset($price_field)) {
      $context['sandbox']['query'][] = "{$price_field}=0+";
    }
    // limits and chunk size
    $context['sandbox']['limit'] = $resources[$mappings[$type]['resource']]['chunk_size'];
    if ($context['sandbox']['limit'] == 0) {
      $context['sandbox']['limit'] = 'NONE';
    }
    $context['results']['chunks'] = 0;

  }

  include_once "phRets/phRets.php";
  $ph_rets = new phRets();

  // call phRets->Connect()
  if (drealty_rets_connect($ph_rets, $conid)) {

    $query = implode('),(', $context['sandbox']['query']);
    $ask_fields = implode(',', $context['results']['ask_fields']);

    /**
     *
     * Refer to @link http://retsdoc.onconfluence.com/display/rets172/7.4+Optional+Request+Arguments for explination of the
     * optional arguments passed to the search query.
     *
     * Refer to @link http://troda.com/projects/phrets/index.php?title=SearchQuery for explination of the SearchQuery Function.
     * phRETs SearchQuery(string $resources, string $class, string $query [, array $options])
     *
     */
    $search = $ph_rets->SearchQuery($resource, $class, "($query)", array("Format" => "COMPACT-DECODED", "Limit" => "{$context['sandbox']['limit']}",
      "Offset" => "{$context['sandbox']['offset']}", "Select" => "$ask_fields", "RestrictedIndicator" => "****", "Count" => "1"));

    $listings = array();
    while ($listing = $ph_rets->FetchRow($search)) {
      $listing['crc32'] = calculate_crc($listing);
      $listings[] = $listing;
    }

    cache_set('drealty_chunk_'. $context['results']['chunks']++, $listings);
    $context['sandbox']['offset'] += count($listings);
    $context['sandbox']['total_records'] = $ph_rets->TotalRecordsFound();


    $ph_rets->FreeResult($search);
    $ph_rets->Disconnect();

  }
  else {
    $context['results']['error'] = TRUE;
    $context['finished'] = 1;
    $error = $ph_rets->Error();
    watchdog('drealty', "drealty encountered an error: (Type: @type Code: @code Msg: @text", array("@type" => $error['type'], "@code" => $error['code'], "@text" => $error['text']), WATCHDOG_ERROR);
  }

  $context['finished'] = !$ph_rets->IsMaxrowsReached(); //$context['sandbox']['offset']/$context['sandbox']['total_records']; //!$ph_rets->IsMaxrowsReached();
  $context['message'] = t('@count listings fetched from RETS server.', array('@count' => $context['sandbox']['offset']));
  $context['results']['counts']['total'] = $context['sandbox']['offset'];



}

function calculate_crc($listing) {
  $crcstring = '';
  foreach ($listing as $key => $value) {
    $crcstring .= strtolower(trim($value));
  }
  return md5($crcstring);
}

/**
 * Insert new listings; update current ones. Batch callback.
 *
 * @param array() $context
 * @return none
 *
 */
function drealty_import_batch_update(&$context) {



  $conid = $context['results']['conid'];
  $resource = $context['results']['resource'];
  $class = $context['results']['class'];
  $type = $context['results']['type'];



  switch ($type) {
    case 'drealty_property':
      $id = "{$context['results']['fields'][$context['results']['mls_field']]['systemName']}";
      $id_field = "{$context['results']['fields'][$context['results']['mls_field']]['cck_field_name']}";
      break;
    case 'drealty_office':
      $id = "{$context['results']['fields'][$context['results']['office_id']]['systemName']}";
      $id_field = "{$context['results']['fields'][$context['results']['office_id']]['cck_field_name']}";
      break;
    case 'drealty_agent':
      $id = "{$context['results']['fields'][$context['results']['agent_id']]['systemName']}";
      $id_field = "{$context['results']['fields'][$context['results']['agent_id']]['cck_field_name']}";
      break;
    case 'drealty_open_house':
      $id = "{$context['results']['fields'][$context['results']['open_house_id']]['systemName']}";
      $id_field = "{$context['results']['fields'][$context['results']['open_house_id']]['cck_field_name']}";
      break;
  }

  if (!count($context['sandbox'])) {
    if ($context['results']['error']) {
      $finished = 1;
      return;
    }

    //load the info for the type
    $context['results']['db_props'] = array();
    $result = db_query("SELECT nid, {$type}_crc32_value, dr_{$id_field}_value AS id FROM {content_type_$type} WHERE {$type}_class_value = '%s'", $context['results']['class']);

    while ($prop = db_fetch_array($result)) {
      $context['results']['db_props'][$prop['id']] = $prop;
    }

    $context['results']['in_rets'] = array();
    if ($context['results']['counts']['total'] === 0) {
      // nothing to update. return out and continue to the next batch operation
      return;
    }

    $context['sandbox']['chunk_idx'] = 0;
    $context['sandbox']['in_rets'] = array();
    $context['sandbox']['index'] = 0;
    $context['sandbox']['img_dir'] = file_directory_path() . '/drealty_img/';

    if (variable_get("drealty_use_img_{$resource}_{$conid}", FALSE)) {
      $context['sandbox']['placeholder'] = array(
        // Most of these fields are probably unnecessary…?
        'title' => t('drealty property image placeholder'),
        'filesize' => 34,
        'mimetype' => 'image/jpeg',
        'description' => '',
        'list' => TRUE,
        'data' => array(
          'alt' => t('drealty property image placeholder'),
          'title' => t('drealty property image placeholder'),
        ),
      );

      $context['sandbox']['file'] = new stdClass();
      $context['sandbox']['file']->filemime = 'image/jpeg';
      $context['sandbox']['file']->filesize = 0 ;
      $context['sandbox']['file']->uid = 0;
      $context['sandbox']['file']->status = FILE_STATUS_PERMANENT;
      $context['sandbox']['file']->timestamp = time();
      $context['sandbox']['file']->filepath = drupal_get_path('module', 'drealty') . '/img/placeholder.jpeg';
    }
    else {
      $context['sandbox']['file'] = FALSE;
    }
  }
  // We're going to define some pointers because the code below gets *really*
  // ugly when we use the full array names.
  $_index = &$context['sandbox']['index'];
  $_counts = &$context['results']['counts'];
  //$_mls_field = &$context['results']['mls_field'];
  $_in_rets = &$context['results']['in_rets'];
  $_db_props = &$context['results']['db_props'];
  $_fields = &$context['results']['fields'];
  $_file = &$context['sandbox']['file'];

  $img_dir = file_directory_path() . '/drealty_img';
  $chunk_name = 'drealty_chunk_' . $context['sandbox']['chunk_idx'];
  $_props = cache_get($chunk_name);
  $chunk_count = count($_props->data);
  $context['finished'] = 0;

  for ($x = 0; $x < 50 && $_index < $chunk_count && $context['finished'] < 1; $x++) {
    $prop = $_props->data[$_index];
    $_in_rets[] = $prop[$id];
    // If this property is not currently in the database OR its hash has changed…
    if (!isset($_db_props[$prop[$id]]) || $_db_props[$prop[$id]]["{$type}_crc32_value"] != $prop['crc32']) {
      if (isset($_db_props[$prop[$id]])) {
        // It's a currently-existing node, so load it.
        // The third param tells node_load() to reset the loaded node cache,
        // reducing memory usage.
        $node = node_load($_db_props[$prop[$id]]['nid'], NULL, TRUE);
        $new = FALSE;
        $_counts['update']++;
      }
      else {
        // It's not in the database, so let's create a new node.
        $node = new stdClass();
        $node->type = $type;
        $node->title = $prop[$id];
        $new = TRUE;
        $_counts['new']++;
      }
      // Now edit the node.
      $loc_data = array(
        'street' => array(),
      );
      foreach ($context['results']['ask_fields'] as $field) {
        // Convert the value if necessary.
        if ($_fields[$field]['dataType'] === 'Date') {
          preg_match('/^(\d{4})-(\d{2})-(\d{2})$/', $prop[$field], $matches);
          $value = gmmktime(0, 0, 0, $matches[2], $matches[3], $matches[1]);
        }
        elseif ($_fields[$field]['dataType'] === 'DateTime') {
          preg_match('/^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})$/', $prop[$field], $matches);
          $value = gmmktime($matches[4], $matches[5], $matches[6], $matches[2], $matches[3], $matches[1]);
        }
        elseif ($_fields[$field]['interpretation'] === 'LookupMulti') {
          $value = explode(',', $prop[$field]);
        }
        else {
          $value = $prop[$field];
        }

        $node_child = 'dr_' . $_fields[$field]['cck_field_name'];
        if (is_array($value)) {
          $cck_value = array();
          foreach ($value as $val) {
            $cck_value[] = array('value' => $val);
          }
          $node->{$node_child} = $cck_value;
        }
        else {
          $node->{$node_child} = array(
            array(
              'value' => $value,
            ),
          );
        }
        // Should we use this for the body field?
        if ($_fields[$field]['correlation'] === 'body') {
          $node->body = $value;
        }
        elseif ($_fields[$field]['correlation'] === 'pic_count' && $_file/*  && $_placeholder */) {
          $node->dr_images = array();
          // Attach the placeholder image
          $prop_dir = "{$img_dir}";
          if (!is_dir($prop_dir) && !mkdir($prop_dir, 0755)) {
            watchdog('drealty', 'Directory @dir doesn\'t exist and couldn\'t be created (probably a permissions problem).', array('@dir' => $prop_dir), WATCHDOG_ERROR);
          }
          else {
            for ($index = 1; $index <= $value; $index++) {
              $fname = "{$prop[$id]}-{$index}.jpeg";
              $dest = "{$prop_dir}/{$fname}";
              // Since file_copy stupidly doesn't let us specify the destination
              // path explicitly, stopping us from copying a file so it has a
              // different filename than the source, we do this workaround.
              $_file->filename = $fname;
              $fclone = clone $_file;
              if (file_copy($fclone, $prop_dir, FILE_EXISTS_REPLACE)) {
                $_file->filepath = $dest;
                drupal_write_record('files', $_file);
                $_placeholder['fid'] = $_file->fid;
                $_placeholder['filename'] = $fname;
                $_placeholder['filepath'] = $dest;
                $node->dr_images[] = $_placeholder;
              }
              else {
                watchdog('drealty', 'File @file couldn\'t be saved (probably a permissions problem).', array('@file' => $dest), WATCHDOG_ERROR);
              }
            }
          }
        }
        elseif (strpos($_fields[$field]['correlation'], 'loc_') === 0) {
          if (preg_match('/^loc_street_(\d+)$/', $_fields[$field]['correlation'], $matches)) {
            if ($value !== '') {
              $loc['street'][intval($matches[1])] = $value;
            }
          }
          else {
            $loc[$_fields[$field]['correlation']] = $value;
          }
        }
      }
      if (variable_get("drealty_use_loc_{$resource}_{$conid}", FALSE)) {
        // Build values for Location field
        $loc_field_name = "{$type}_loc";
        $node->{$loc_field_name} = array(
          array(),
        );
        foreach (array('postal_code', 'province', 'city') as $field) {
          $loc_field = 'loc_' . $field;
          if (isset($loc[$loc_field])) {
            $node->{$loc_field_name}[0][$field] = $loc[$loc_field];
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
          'value' => $context['results']['class'],
        ),
      );
      $crc_field_name = "{$type}_crc32";
      $node->{$crc_field_name} = array(
        array(
          'value' => $prop['crc32'],
        ),
      );
      $conid_field_name = "{$type}_conid";
      $node->{$conid_field_name} = array(
        array(
          'value' => $conid,
        ),
      );
      $node = node_submit($node);
      $node->revision = FALSE;
      if ($new) {
        watchdog('drealty', 'Creating node @nid (@title).', array('@nid' => $node->nid, '@title' => $node->title));
      }
      else {
        watchdog('drealty', 'Updating node @nid (@title).', array('@nid' => $node->nid, '@title' => $node->title));
      }
      node_save($node);
      // Clear the $_SESSION['messages'] to keep it from taking up a bunch of
      // memory
      unset($_SESSION['messages']);
      unset($node);
    }
    // Else, leave the node alone
    else {
      $_counts['keep']++;
    }


    $context['finished'] = ++$_counts['checked'] / $_counts['total'];


    if (++$_index >= $chunk_count) {
      // Prepare for next chunk;
      $_index = 0;
      // Remove this chunk from cache;
      cache_clear_all($chunk_name, 'cache');
      $context['sandbox']['chunk_idx']++;
    }
  } // End of for loop
  $context['message'] = t('Processed @index of @count (@new created, @update updated)', array('@index' => $_counts['checked'], '@count' => $_counts['total'], '@new' => $_counts['new'], '@update' => $_counts['update']));
}

/**
 * Delete old listings. Batch callback.
 */
function drealty_import_batch_del(&$context) {
  if (!count($context['sandbox'])) {
    if ($context['results']['error']) {
      $finished = 1;
      return;
    }
    // Both of these arrays contain MLS numbers.
    $context['sandbox']['to_delete'] = array_values(array_diff(array_keys($context['results']['db_props']), $context['results']['in_rets']));
    $context['sandbox']['to_delete_count'] = count($context['sandbox']['to_delete']);
    if ($context['sandbox']['to_delete_count'] === 0) {
      return;
    }
    $context['sandbox']['index'] = 0;
  }
  $context['finished'] = 0;
  for ($x = 0; $x < 25 && $context['finished'] < 1; $x++) {
    $node = node_load($context['results']['db_props'][$context['sandbox']['to_delete'][$context['sandbox']['index']]]['nid'], NULL, TRUE);
    node_delete($node->nid);
    unset($_SESSION['messages']);
    $context['finished'] = ++$context['sandbox']['index'] / $context['sandbox']['to_delete_count'];
  }
  $context['results']['counts']['del'] = $context['sandbox']['index'];
  $context['message'] = t('Deleted @index of @count.', array('@index' => $context['sandbox']['index'], '@count' => $context['sandbox']['to_delete_count']));
}

/**
 * Display batch processing result to user. Batch callback.
 */
function drealty_import_batch_finished($success, $results, $operations) {
  if (count($results) && $results['counts']['checked'] >= $results['counts']['total']) {

    $conid = $results['conid'];
    $resource = $results['resource'];
    $class = $results['class'];
    $type = $results['type'];

    $classes = drealty_classes_fetch($conid, $resource);
    $class = $classes[$class];
    $class['lastupdate'] = time();
    drupal_write_record("drealty_classes", $class, array('cid'));
    if (variable_get('cron_semaphore', FALSE)) {
      watchdog('drealty', 'Class update for @class completed. @new created, @update updated, @del deleted, @keep unchanged.', array('@class' => $results['class'], '@new' => $results['counts']['new'], '@update' => $results['counts']['update'], '@del' => $results['counts']['del'], '@keep' => $results['counts']['keep']));
      cache_clear_all('drealty_classes', 'cache', TRUE);

    }
    else {
      if ($results['error']) {
        drupal_set_message(t('Class update for @class did not finish due to a problem when connecting to the RETS server. See the <a href="!log">recent log entries page</a> for more information.', array('@class' => $results['class'], '!log' => url('admin/reports/dblog'))), 'error');
      }
      else {
        drupal_set_message(t('Class update for @class completed. @new created, @update updated, @del deleted, @keep unchanged.', array('@class' => $results['class'], '@new' => $results['counts']['new'], '@update' => $results['counts']['update'], '@del' => $results['counts']['del'], '@keep' => $results['counts']['keep'])));
      }
    }
  }
  else {
    drupal_set_message(t('It appears that class updating failed.'));
  }
}

function drealty_process_images_batch($nids, &$context) {
  if (!count($context['sandbox'])) {
    $context['sandbox']['count'] = count($nids);
    if ($context['sandbox']['count'] === 0) {
      return;
    }
    $context['sandbox']['index'] = 0;
    $context['sandbox']['nids'] = $nids;
  }

  $context['finished'] = 0;
  include_once "phRets/phRets.php";
  $ph_rets = new phRets();

  // call phRets->Connect()
  if (drealty_rets_connect($ph_rets, 1)) {
    for ($x = 0; $x < 25 && $context['finished'] < 1; $x++) {

      $node = node_load($context['sandbox']['nids'][$context['sandbox']['index']], NULL, TRUE);

      foreach ($node->dr_images as $image) {


        if (preg_match('((\d+)-(\d+)\.jpeg)', $image['filename'], $matches) && $image['file_size'] == 0) {

          $result = $ph_rets->GetObject("Property", "Photo", $matches[1], $matches[2]);

          //error_log(print_r($result, TRUE));
          if (!$result[0]['Success']) {
            watchdog('drealty', 'An attempt to fetch the photo @id failed.', array('@id' => "{$matches[1]}:{$matches[2]}"), WATCHDOG_ERROR);
            return FALSE;
          }
          // Write the image
          file_put_contents($image['filepath'], $result[0]['Data']);
          // Update {files}
          $record = array(
            'filename' => $image['filename'],
            'filesize' => isset($data) ? strlen($result[0]['Data']) : filesize($dest),
          );
          drupal_write_record('files', $record, 'filename');
        }
        drealty_disconnect($ph_rets);

      }
    }
    unset($_SESSION['messages']);
    $context['finished'] = ++$context['sandbox']['index'] / $context['sandbox']['count'];
  }
  $context['message'] = t('Processed @index of @count.', array('@index' => $context['sandbox']['index'], '@count' => $context['sandbox']['count']));
}
/**
 * Delete property listing nodes. Batch callback.
 */
function drealty_flush_batch($nids, $class, &$context) {
  if (!count($context['sandbox'])) {
    $context['sandbox']['count'] = count($nids);
    if ($context['sandbox']['count'] === 0) {
      return;
    }
    $context['sandbox']['index'] = 0;
    $context['sandbox']['nids'] = $nids;
    $context['results']['class'] = $class;
  }

  // $context['finished'] is set to 1 at the onset of each call to a batch
  // function, apparently. Set it to zero so that the for loop below will
  // properly run its first run.
  $context['finished'] = 0;

  for ($x = 0; $x < 250 && $context['finished'] < 1; $x++) {
    //$node = node_load($context['sandbox']['nids'][$context['sandbox']['index']], NULL, TRUE);
    node_delete($context['sandbox']['nids'][$context['sandbox']['index']]);
    //unset($_SESSION['messages']);
    $context['finished'] = ++$context['sandbox']['index'] / $context['sandbox']['count'];
  }
  $context['message'] = t('Deleted @index of @count.', array('@index' => $context['sandbox']['index'], '@count' => $context['sandbox']['count']));
}

/**
 * Display result to the user. Batch callback.
 */
function drealty_flush_batch_finished($success, $results, $operations) {
  // Set the last_update times for the relative classes back to 'never'
  $conid = $results['conid'];

  drupal_set_message('The property listings have been deleted.');
  //drupal_goto('admin/drealty/connections');
}
function drealty_process_images_batch_finished($success, $results, $operations) {

  drupal_set_message('The Image data has been processed.');
  //drupal_goto('admin/drealty/connections');
}
/**
 * Temporarily disable module hooks on a per-script basis
 * Useful because it doesn't disable modules in system table
 * @param array $modules
 * @param array $hooks
 */
function drealty_disable_modules_hooks($modules, $hooks) {
  //test
  $orig_modules = $list = module_list();
  foreach ($modules as $module) {
    unset($list[$module]);
  }
  // Reset module list
  module_list(FALSE, FALSE, TRUE, $list);
  // Reset implementations
  module_implements('', FALSE, TRUE);
  // Add each hook to implementations, which will call our modified module list
  foreach ($hooks as $hook) {
    module_implements($hook);
  }
  // Put module list back to normal, so other hooks work normally
  module_list(FALSE, FALSE, TRUE, $orig_modules);
}

