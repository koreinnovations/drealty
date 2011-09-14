<?php
function drealty_base_admin_menu() {
  $output = "Some content to go here.";
  $output .="<ul><li>".  l("Connections", "admin/config/drealty/connection") . "</li></ul>";
  return array('#markup' => $output);
}

function drealty_connection_list_form() {
  // grab the connections from the db
  $result = db_query("SELECT * FROM {drealty_connections}");
  $connections = array();
  $output = "";

  $connections = $result->fetchAll();

  if (!empty($connections)) {
    $rows = array();
    $header = array('Connection ID', 'Connection Name', 'Status', 'Operations');
    foreach ($connections as $connection) {
      $operation = l("Edit", "admin/config/drealty/connection/edit/$connection->conid") . ' | ' . l("Delete", "admin/config/drealty/connection/delete/{$connection->conid}");
      if ($connection->active) {
        $operation .= ' | ' . l("Deactivate", "admin/config/drealty/connection/deactivate/{$connection->conid}");
        $operation .= ' | ' . l("Export", "admin/config/drealty/connection/export/{$connection->conid}");
        $operation .= ' | ' . l("Data Management", "admin/config/drealty/connection/manual_update/{$connection->conid}");
        $operation .= ' <br /> ' . "Configure: " . l("Properties", "admin/config/drealty/connection/resource/{$connection->conid}/property");
        $operation .= ' | ' . l("Open Houses", "admin/config/drealty/connection/resource/{$connection->conid}/open_house");
        $operation .= ' | ' . l("Offices", "admin/config/drealty/connection/resource/{$connection->conid}/office");
        $operation .= ' | ' . l("Agents", "admin/config/drealty/connection/resource/{$connection->conid}/agent");
      } else {
        $operation .= ' | ' . l("Activate", "admin/config/drealty/connection/activate/{$connection->conid}");
      }
      $status = $connection->active ? 'Active' : 'Inactive';
      $rows[] = array($connection->conid, $connection->name, $status, $operation);
    }
    $output = theme('table', array('header' => $header, 'rows' => $rows)); 
  } else {
    $output = "<div>There are no Connections setup. Please setup a new Connection.</div>";
  }

  $output .= "<br /><div>";
  $output .= l("Add Connection", "admin/config/drealty/connection/add");
  $output .= " | ";
  $output .= l("Import Connection", "admin/config/drealty/connection/import");
  $output .= "</div>";

  return array('#markup' => $output);;
}

function drealty_connection_setup_form($form, &$form_state, $conid = NULL) {

  if (isset($conid)) {
    $value  = db_query("SELECT * FROM {drealty_connections} WHERE conid = :conid", array( ':conid' => $conid))->fetch();
  }

  $form['#redirect'] = '/admin/config/drealty/connection';
  $form['connection'] = array('#type' => 'fieldset', '#title' => t('Connection Information'));
  $form['connection']['connection_id'] = array('#type' => 'value', '#value' => $conid);
  $form['connection']['connection_name'] = array(
      '#type' => 'textfield',
      '#title' => t('Connection Name'),
      '#description' => t('Name to identify the connection in the administration pages.'),
      '#required' => TRUE,
      '#size' => 80,
      '#default_value' => isset($value->name) ? $value->name : '',
  );
  $form['connection']['login_url'] = array(
      '#type' => 'textfield',
      '#title' => t('Login URL'),
      '#default_value' => isset($value->login_url) ? $value->login_url : '',
      '#description' => t('Login URL given to you by your RETS provider. <em>i.e. (http://demo.crt.realtors.org:6103/rets/login)</em>'),
      '#required' => TRUE,
      '#size' => 80,
  );
  $form['connection']['connection_username'] = array(
      '#type' => 'textfield',
      '#title' => t('Username'),
      '#default_value' => isset($value->username) ? $value->username : '',
      '#size' => 40,
      '#required' => TRUE,
  );
  $form['connection']['connection_password'] = array(
      '#type' => 'textfield',
      '#title' => t('Password'),
      '#default_value' => isset($value->password) ? $value->password : '',
      '#size' => 40,
      '#required' => TRUE,
  );

  $form['connection']['user_agent'] = array(
      '#type' => 'textfield',
      '#title' => t('User Agent String'),
      '#description' => t('A User Agent String.'),
      '#required' => TRUE,
      '#size' => 80,
      '#default_value' => isset($value->ua_string) ? $value->ua_string : 'dRealty/1.0',
  );
    $form['connection']['user_agent_password'] = array(
      '#type' => 'textfield',
      '#title' => t('User Agent Password'),
      '#description' => t('(Leave blank if you don\'t have one).'),
      '#required' => FALSE,
      '#size' => 40,
      '#default_value' => isset($value->ua_password) ? $value->ua_password : 'dRealty/1.0',
  );
  $rets_versions = array('1.0' => '1.0', '1.5' => '1.5', '1.7' => '1.7', '1.7.2' => '1.7.2');
  $form['connection']['connection_version'] = array(
      '#type' => 'select',
      '#title' => t('RETS Version'),
      '#options' => $rets_versions,
      '#required' => TRUE,
      '#default_value' => isset($value->version) ? $value->version : '1.5',
  );
  $form['connection']['force_basic_auth'] = array(
      '#type' => 'radios',
      '#title' => t('Force Basic Authentication'),
      '#options' => array(TRUE => 'yes', FALSE => 'no'),
      '#required' => TRUE,
      '#default_value' => isset($value->force_basic_auth) ? $value->force_basic_auth : 0,
  );
  $form['connection']['use_compression'] = array(
      '#type' => 'radios',
      '#title' => t('Use Compression'),
      '#options' => array(TRUE => 'yes', FALSE => 'no'),
      '#required' => TRUE,
      '#default_value' => isset($value->use_compression) ? $value->use_compression : 0,
  );
  $form['submit'] = array('#type' => 'submit', '#value' => t('Save'));
  return $form;
}

/*
 * Implementation of hook_form_submit() -- insert/update connection
 */

function drealty_connection_setup_form_submit($form, &$form_state) {
  $form_state['redirect'] = 'admin/config/drealty/connection';
  $update = isset($form_state['values']['connection_id']) ? array('conid') : array();
  $function = isset($form_state['values']['connection_id']) ? 'updating' : 'creating';

  $connection = array(
      'conid' => isset($form_state['values']['connection_id']) ? $form_state['values']['connection_id'] : NULL,
      'name' => $form_state['values']['connection_name'],
      'login_url' => $form_state['values']['login_url'],
      'username' => $form_state['values']['connection_username'],
      'password' => $form_state['values']['connection_password'],
      'ua_string' => $form_state['values']['user_agent'],
      'ua_password' => $form_state['values']['user_agent_password'],
      'version' => $form_state['values']['connection_version'],
      'force_basic_auth' => $form_state['values']['force_basic_auth'],
      'use_compression' => $form_state['values']['use_compression'],
  );

  $result = drupal_write_record('drealty_connections', $connection, $update);

  if ($result) {
    $severity = 'notice';
    switch ($result) {
      case SAVED_NEW:
        $message = t('%connection_name has been created.', array('%connection_name' => $form_state['values']['connection_name']));

        break;
      case SAVED_UPDATED:
        $message = t('%connection_name has been updated.', array('%connection_name' => $form_state['values']['connection_name']));
        break;
    }
  } else {
    $message = t('dRealty IDX has encountered a problem while @function %connection_name. Please contact the system administrator if this continues.',
        array('@function' => $function, '%connection_name' => $form_state['values']['connection_name']));
    $severity = 'error';
  }

  cache_clear_all('drealty_connections', 'cache');
  watchdog('drealty', $message);
  drupal_set_message($message, $severity);
}


function drealty_activate_connection($form, &$form_state, $conid = NULL) {

  if (!isset($conid)) {
    drupal_set_message(t("Must have a connection selected."), 'error');
  } else {

    $rets = new drealtyConnection();

    if( $rets->connect($conid)) {

      $server = $rets->get_phrets()->GetServerInformation();
      $resources = $rets->get_phrets()->GetMetadataResources();
      $login_url = $rets->get_phrets()->GetLoginURL();
      $server_software = $rets->get_phrets()->GetServerSoftware();
      $rets_version = $rets->get_phrets()->GetServerVersion();
      $caps = $rets->get_phrets()->GetAllTransactions();
      $capabilities = '';
      foreach ($caps as $cap) {
        $capabilities .= $cap . ', ';
      }
      $supported_caps = substr($capabilities, 0, strlen($capabilities) - 2);
      $auth_supported = $rets->get_phrets()->CheckAuthSupport('digest') ? "Digest Authentication is Supported" : "(unknown - assuming Basic)";



      $form['server_info'] = array('#type' => 'fieldset', '#title' => 'Server Information');
      $form['server_info']['properties'] = array(
          '#type' => 'item',
          '#markup' => "<table border='0' cellpadding='1' cellspacing='1' width='100%'>\n" .
          " <tr><td width='25%' valign='top'><strong>RETS Server:</strong></td><td width='75%' valign='top'>{$server['SystemDescription']}</td></tr>\n" .
          " <tr><td width='25%' valign='top'><strong>RETS System ID:</strong></td><td width='75%' valign='top'>{$server['SystemID']}</td></tr>\n" .
          " <tr><td width='25%' valign='top'><strong>Login URL:</strong></td><td width='75%' valign='top'>$login_url</td></tr>\n" .
          " <tr><td width='25%' valign='top'><strong>RETS Version:</strong></td><td width='75%' valign='top'>{$rets_version}</td></tr>\n" .
          " <tr><td width='25%' valign='top'><strong>Server Software:</strong></td><td width='75%' valign='top'>{$server_software}</td></tr>\n" .
          " <tr><td width='25%' valign='top'><strong>Authentication Supported:</strong></td><td width='75%' valign='top'>{$auth_supported}</td></tr>\n" .
          " <tr><td width='25%' valign='top'><strong>Transactions Supported:</strong></td><td width='75%' valign='top'>{$supported_caps}</td></tr>\n" .
          "</table>"
      );       
     
      $form['conid'] = array('#type' => 'value', '#value' => $conid);
      $form['activate'] = array(
          '#type' => 'submit',
          '#value' => t('Activate'),
      );
    }
    $rets->disconnect();
  }
  return $form;
}

function drealty_activate_connection_submit($form, &$form_state) {

  $form_state['redirect'] = 'admin/config/drealty/connection';
  $connection  = db_query("SELECT * FROM {drealty_connections} WHERE conid = :conid", array( ':conid' => $form_state['values']['conid']))->fetch();
  $connection->active = true;
  
  drupal_write_record('drealty_connections', $connection, 'conid');

  //go ahead and grab all the lookup values and store them
  watchdog('drealty', "Activated Connection: @conname", array('@conname' => $connection->name));
  drupal_set_message(t("Activated Connection: @conname", array('@conname' => $connection->name)));
}
/*
 * Menu Callback -- Deactivates a connection
 */
function drealty_deactivate_connection($conid = NULL) {
  if (isset($conid)) {
  $connection  = db_query("SELECT * FROM {drealty_connections} WHERE conid = :conid", array( ':conid' => $conid))->fetch();
  $connection->active = false;
    drupal_write_record('drealty_connections', $connection, 'conid');

    watchdog('drealty', "Deactivated Connection: @conname", array('@conname' => $connection->name));
    drupal_set_message(t("Deactivated Connection: @conname", array('@conname' => $connection->name)));
    drupal_goto('admin/config/drealty/connection');
  } else {
    drupal_set_message(t("Error deactivating connection."), 'error');
    drupal_goto('admin/config/drealty/connection');
  }
}

function drealty_configure_resource($form, &$form_state, $conid = NULL, $resource = NULL) {

  if (!isset($conid)) {
    drupal_set_message(t("Must have a connection slected."), 'error');
    drupal_goto('/admin/config/drealty/connection');
  }
  $dr = new drealtyResources();
// grab the mappings and see if we have one already
  $mappings = $dr->GetResourceMappings($conid);

  $results = $dr->FetchResources($conid);
  $resources = array();
  foreach ($results as $res) {
    $resources[$res->systemname] = $res->systemname;
  }

  $form['mid'] = array('#type' => 'value', '#value' => isset($mappings["drealty_$resource"]) ? $mappings["drealty_$resource"]->mid : NULL);
  $form['conid'] = array('#type' => 'value', '#value' => $conid);
  $form['resource'] = array('#type' => 'value', '#value' => $resource);
  $form['prop_resource'] = array(
      '#type' => 'select',
      '#title' => t('@resource Resource', array('@resource' => ucwords(str_replace("_", " ", $resource)))),
      '#default_value' => isset($mappings["drealty_$resource"]) ? $mappings["drealty_$resource"]->resource : NULL,
      '#options' => $resources,
  );
  $form['select_prop_resource'] = array(
      '#type' => 'button',
      '#value' => t('Select'),
      '#submit' => array('drealty_connection_configure'),
      '#suffix' => '<br /><br />',
  );

  if (isset($form_state['input']['prop_resource']) || isset($mappings["drealty_$resource"])) {
    if (isset($form_state['input']['prop_resource'])) {
      $results = $dr->FetchClasses($conid, $form_state['input']['prop_resource']);
    } else {
      $results = $dr->FetchClasses($conid, $mappings["drealty_$resource"]->resource);
    }

    $life_opts = array(0 => t('Every time')) + drupal_map_assoc(array(
          900, // 15 mins
          1800, // 30 mins
          3600, // 1 hour
          7200, // 2 hours
          10800, // 3 hours
          21600, // 6 hours
          28800, // 8 hours
          43200, // 12 hours
          86400, // 1 day
          172800, // 2 days
        ), 'format_interval');

    $form['classes']['#theme'] = 'drealty_classes_form';
    $form['classes']['#tree'] = TRUE;
    foreach ($results as $key => $class) {

      $form['classes'][$key]['name'] = array('#markup' => $class->systemname);
      $form['classes'][$key]['description'] = array('#markup' => "{$class->standardname} {$class->description}");

      $form['classes'][$key]['cname'] = array('#value' => $class->systemname);
      $form['classes'][$key]['sname'] = array('#value' => "{$class->standardname} {$class->description}");

      $form['classes'][$key]['enabled'] = array('#type' => 'checkbox', '#default_value' => $class->enabled, '#attributes' => $class->enabled ? array('checked' => 'checked') : NULL);

      $form['classes'][$key]['lifetime'] = array('#type' => 'select', '#options' => $life_opts, '#default_value' => isset($class->lifetime) ? $class->lifetime : 28000,);
      $form['classes'][$key]['rid'] = array('#type' => 'value', '#value' => $class->rid);
      $form['classes'][$key]['conid'] = array('#type' => 'value', '#value' => $class->conid);
      $form['classes'][$key]['cid'] = array('#type' => 'value', '#value' => $class->cid);
      $form['classes'][$key]['classname'] = array('#type' => 'value', '#value' => $class->systemname);
      $form['classes'][$key]['standardname'] = array('#type' => 'value', '#value' => $class->standardname);
    }

    $form['save_changes'] = array(
        '#type' => 'submit',
        '#value' => t('Save Changes'),
    );
  }
  return $form;
}

function drealty_configure_resource_submit($form, &$form_state) {

  $resource = $form_state['values']['resource'];

// update the resource mapping for this connection
  $mapping = array(
      'mid' => isset($form_state['values']['mid']) ? $form_state['values']['mid'] : NULL,
      'conid' => $form_state['values']['conid'],
      'resource' => $form_state['values']['prop_resource'],
      'node_type' => "drealty_$resource",
  );
// update or insert
  if (isset($mapping['mid'])) {
    drupal_write_record('drealty_resource_mappings', $mapping, array('mid'));
  } else {
    unset($mapping['mid']);
    drupal_write_record('drealty_resource_mappings', $mapping, array());
  }

  $select_opts = array();
  // update the classes
  foreach ($form_state['values']['classes'] as $class) {
    drupal_write_record('drealty_classes', $class, array('cid'));
    $select_opts[] = "{$class['classname']}|{$class['standardname']}";
  }


  $conid = $form_state['values']['conid'];

  // clear out the cached data
  cache_clear_all("drealty_classes_", 'cache', TRUE);
  cache_clear_all("drealty_mappings_$conid", 'cache');
  // set a succes message
  drupal_set_message(t('The Configuration options have been Saved.'));
}

function drealty_configure_resource_fields($form, &$form_state, $conid = NULL, $resource = NULL ) {
  if (!isset($conid)) {
    drupal_set_message(t("Must have a connection slected."), 'error');
    $markup = "Please Setup a Connection ". l("here", "admin/config/drealty/connection/add");
    return array("#markup" => $markup);
  }

  // grab the groups so we can use them
 // $groups = fieldgroup_groups("drealty_$resource", TRUE);

  $dr = new drealtyResources();
  $dc = new drealtyConnection();

  // grab the mappings and see if we have one already
  $mappings = $dr->GetResourceMappings($conid);

  if (!isset($mappings["drealty_$resource"])) {
    drupal_set_message(t('You must configure the @resource Resource type first.', array("@resource" => ucwords($resource))), 'error');
    drupal_goto("admin/drealty/connection/resource/$conid/$resource");
  }
  $res = $mappings["drealty_$resource"]->resource;
  $fields = $dr->FetchFields($conid, $res);
  $connections = $dc->FetchConnections();
  $connection = $connections[$conid];


  $drupal_field_opts = array();
  $none = array();
  $tmp_array = array();
  $none['none'] = 'none';
  foreach ($connections as $con) {
    $default_fields = $dr->FetchActiveFields($con->conid, $res);
    $temp_field_opts = array();

    foreach ($default_fields as $drupal_field) {
      $temp_field_opts[$drupal_field->field_name] = $drupal_field->field_label;
    }
    $tmp_array = array_merge($temp_field_opts, $tmp_array);
  }

  $drupal_field_opts = array_merge($none, $tmp_array);

  $form['form_message'] = array( '#markup' => t('Use the form below to select which fields you would like to import from the RETS feed.'), '#prefix' => '<div style="padding: 1em;">', '#suffix' => '</div>');
  $form['conid'] = array('#type' => 'value', '#value' => $conid);
  $form['resource'] = array('#type' => 'value', '#value' => $res);
  $form['node_type'] = array('#type' => 'value', '#value' => "drealty_$resource");
  $form['fields']['#tree'] = TRUE;



  if (count($connections) > 1) {
    $form['fields']['#theme'] = 'drealty_fields_alt_form';
  } else {
    $form['fields']['#theme'] = 'drealty_fields_form';
  }



  foreach ($fields as $field) {
    $form['fields'][$field->systemname]['display'] = array(
        '#type' => 'checkbox',
        '#title' => t('@sysname', array('@sysname' => $field->longname)),
        '#description' => t('@longname <em>(appears in: @classes)</em>', array('@longname' => ucwords(strtolower($field->systemname)), '@classes' => $field->classes)),
        '#default_value' => $field->display,
    );
    if (!$field->mapped_field) {
      $form['fields'][$field->systemname]['field_name'] = array(
          '#type' => 'textfield',
          '#default_value' => $field->field_name ? $field->field_name : strtolower(substr($field->systemname, 0, 29)),
          '#field_prefix' => '<span dir="ltr">dr_',
          '#field_suffix' => '</span>',
          '#maxlength' => 29,
          '#size' => 29,
          '#description' => t('Field name (a-z, 0-9, _)'),
      );
      $form['fields'][$field->systemname]['field_label'] = array(
          '#type' => 'textfield',
          '#default_value' => $field->field_label ? $field->field_label : ucwords(strtolower($field->longname)),
          '#size' => 30,
          '#description' => t('&nbsp;')
      );
    } else {
      $form['fields'][$field->systemname]['field_name'] = array(
          '#type' => 'item',
          '#value' => $field->field_name ? $field->field_name : strtolower(substr($field->systemname, 0, 29)),
      );
      $form['fields'][$field->systemname]['field_label'] = array(
          '#type' => 'item',
          '#value' => $field->field_label ? $field->field_label : $field->longname,
      );
    }
    $form['fields'][$field->systemname]['field_map'] = array(
        '#type' => 'select',
        '#options' => $drupal_field_opts,
        '#default_value' => $field->field_name ? $field->field_name : 'none',
        '#description' => t('&nbsp;'),
    );
  }

  $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save Configuration'),
  );
  return $form;
}

function drealty_configure_resource_fields_submit($form, &$form_state) {

  $dr = new drealtyResources();
  $dc = new drealtyConnection();

  $conid = $form_state['values']['conid'];
  $resource = $form_state['values']['resource'];
  $node_type = $form_state['values']['node_type'];
  $fields = $dr->FetchFields($conid, $resource);

  $connections = $dc->FetchConnections();
  $connection = $connections[$conid];

  $default_fields = array();
  foreach ($connections as $con) {
    $tmp_fields = $dr->FetchActiveFields($con->conid, $resource);
    $default_fields = array_merge($tmp_fields, $default_fields);
  }


  foreach ($fields as $id => $field) {

    if ($form_state['values']['fields'][$id]['display'] == 1 && $field->display != 1) {
      //check to see if we need to create  a new field or map it to an existing one.
      if ($form_state['values']['fields'][$id]['field_map'] === 'none') {

        $field_name =  'dr_' . $form_state['values']['fields'][$id]['field_name'];
        $new_field = array(
            'field_name' => $field_name,
            'locked' => TRUE,
        );
        $new_instance = array(
            'field_name' => $field_name,
            'label' => $form_state['values']['fields'][$id]['field_label'],
            'entity_type' => 'node',
            'bundle' => $node_type,
        );

          if ((!in_array($field->interpretation, array('Lookup', 'LookupMulti')) && in_array($field->datatype, array('Boolean', 'Tiny', 'Small', 'Int', 'Long'))) || ($field->datatype === 'Decimal' && $field->field_precision == 0)) {
            $new_field['type'] = 'number_integer';

          } elseif (!in_array($field->interpretation, array('Lookup', 'LookupMulti')) && $field->datatype === 'Decimal') {

            $new_field['type'] = 'number_decimal';

            // Note that RETS and this CCK field are using the term "precision"
            // differently
            $new_field['settings'] = array('precision' => $field->max_length, 'scale' => $field->field_precision);

          }  else {
            // Assume all others are Character
            $new_field['type'] =$field->max_length > 127 ? 'text_long' : 'text';
            if($field->interpretation == "LookupMulti") {
              $new_field['cardinality'] = -1;
            }

          }

           field_create_field($new_field);
           field_create_instance($new_instance);

       
        $record = array(
            'fid' => $field->fid,
            'display' => 1,
            'field_name' => $form_state['values']['fields'][$id]['field_name'],
            'field_label' => $form_state['values']['fields'][$id]['field_label'],
            'mapped_field' => 0,
        );
        $status = "Creating";
      } else {
        //map this field to an existing cck field
        $map_field = $form_state['values']['fields'][$id]['field_map'];
        $match = NULL;
        foreach ($default_fields as $item) {
          if ($item['field_name'] === $map_field) {
            $match = $item;
            break;
          }
        }
        if (isset($match)) {
          $record = array(
              'fid' => $field->fid,
              'display' => 1,
              'field_name' => $match['field_name'],
              'field_label' => $match['field_label'],
              'mapped_field' => 1,
          );
        }
        $status = "Mapping";
      }
      
      drupal_write_record('drealty_fields', $record, 'fid');
    }
    // Else if the field is unchecked on the form but we ARE currently using it…
    elseif (!$form_state['values']['fields'][$id]['display'] && $field->display) {

      if (!$field->mapped_field) {

        drupal_set_message(t('Removing field %label (@id).', array('%label' => $field->field_label, '@id' => $id)));
        $field_name = 'dr_' . $field->field_name;
        field_delete_field($field_name);
        $instance = field_info_instance('node', $field_name, $node_type);
        field_delete_instance($instance);

        // If that field was the image count field, delete the image field too.
//        if ($field['correlation'] === 'pic_count') {
//          drupal_set_message(t('Removing <em>Images</em> field.'));
//          content_field_instance_delete('dr_images', $node_type, FALSE);
//        }
      }
      $record = array(
          'fid' => $field->fid,
          'display' => 0,
          'mapped_field' => 0,
          'field_name' => '',
          'field_label' => '',
      );

      drupal_write_record('drealty_fields', $record, 'fid');
    } elseif ($form_state['values']['fields'][$id]['display'] && $field->display && $field->mapped_field && $form_state['values']['fields'][$id]['field_map'] === 'none') {
      $record = array(
          'fid' => $field->fid,
          'display' => 0,
          'field_name' => '',
          'field_label' => '',
          'mapped_field' => 0,
      );
      drupal_set_message(t("Removing the Mapping from @field_name to @mapping_name.", array("@field_name" => $field->systemName, "@mapping_name" => $field->field_label)));
      drupal_write_record('drealty_fields', $record, 'fid');
    }
  }
  drupal_set_message(t('The configuration options have been saved.'));

  cache_clear_all("drealty_fields_{$resource}_{$conid}", 'cache');
  cache_clear_all("drealty_fields_active_{$resource}_{$conid}", 'cache');
}

function drealty_configure_resource_correlate($form, &$form_state, $conid = NULL, $resource = NULL) {
  $corrs = array();

  switch ($resource) {
    case 'property':
      $corrs = array(
          'none' => t('None'),
          'mls_id' => t('MLS ID number'),
          'body' => t('Description (node body)'),
          'pic_count' => t('Picture count'),
          'sale_status' => t('Sale status'),
          'price' => t('Price'),
          'agent_id' => t('Agent ID'),
          'office_id' => t('Office ID'),
          'loc_street_1' => t('Location: Street address 1'),
          'loc_street_2' => t('Location: Street address 2'),
          'loc_street_3' => t('Location: Street address 3'),
          'loc_street_4' => t('Location: Street address 4'),
          'loc_street_5' => t('Location: Street address 5'),
          'loc_street_6' => t('Location: Street address 6'),
          'loc_city' => t('Location: City'),
          'loc_province' => t('Location: State'),
          'loc_postal_code' => t('Location: ZIP code'),
      );
      break;
    case 'agent':
      $corrs = array(
          'none' => t('None'),
          'agent_id' => t('Agent ID'),
          'agent_first_name' => t("First Name"),
          'agent_last_name' => t("Last Name"),
          'office_id' => t("Agent's Office iD"),
          'agent_type' => t("Agent Type"),
      );
      break;
    case 'office':
      $corrs = array(
          'none' => t('None'),
          'office_id' => t("The Office iD"),
          'office_type' => t("Office Type | Office Status"),
          'office_name' => t("Office Name"),
          'loc_street_1' => t('Location: Street address 1'),
          'loc_street_2' => t('Location: Street address 2'),
          'loc_street_3' => t('Location: Street address 3'),
          'loc_street_4' => t('Location: Street address 4'),
          'loc_street_5' => t('Location: Street address 5'),
          'loc_street_6' => t('Location: Street address 6'),
          'loc_city' => t('Location: City'),
          'loc_province' => t('Location: State'),
          'loc_postal_code' => t('Location: ZIP code'),
      );
      break;
    case 'open_house':
      $corrs = array(
          'none' => t('None'),
          'open_house_id' => t('Open House ID'),
          'open_house_mls_id' => t("MLS ID number"),
          'sale_status' => t('Sale status'),
          'agent_id' => t('Agent ID'),
          'office_id' => t('Office ID'),
      );
      break;
  }

  $dr = new drealtyResources();
// grab the mappings and see if we have one already
  $mappings = $dr->GetResourceMappings($conid);
  $current_resource = $mappings["drealty_{$resource}"];
  if (!isset($current_resource)) {
    drupal_set_message(t('You must configure the @resource Resource type first.', array("@resource" => ucwords($resource))), 'error');
    drupal_goto("admin/config/drealty/connection/resource/$conid/$resource");
  }
  $res = $current_resource->resource;

  $form['conid'] = array('#type' => 'value', '#value' => $conid);
  $form['resource'] = array('#type' => 'value', '#value' => $resource);

  $fields = $dr->FetchActiveFields($conid, $res);

  $form['fields']['#tree'] = TRUE;
  $form['fields']['#theme'] = 'drealty_fmanage_form';

  foreach ($fields as $field) {
    $form['fields'][$field->systemname]['label'] = array('#markup' => t('@sysname (@longname)', array('@sysname' => $field->field_name, '@longname' => $field->field_label)));
    $form['fields'][$field->systemname]['classes'] = array('#markup' => implode(', ', array_map('check_plain', explode(', ', $field->classes))));
    $form['fields'][$field->systemname]['correlation'] = array('#type' => 'select', '#options' => $corrs, '#default_value' => $field->correlation === '' ? 'none' : $field->correlation);

  }


  if ($resource == 'property') {
    $form['use_img'] = array(
        '#type' => 'checkbox',
        '#title' => t('Add an Image Field to property nodes to store property images'),
    );

    $dc = new drealtyConnection();
    $connections = $dc->FetchConnections();

    if (count($connections) > 1) {

    } else if (count($connections) == 1) {
      $connection = array_shift($connections);
      $fields = $dr->FetchActiveFields($connection->conid, 'Property');
      $options = array();
      foreach ($fields as $field) {
        $options[$field->systemname] = $field->systemname;
      }
    }
    $form['use_img_field_mapping'] = array(
        '#type' => 'checkbox',
        '#title' => t('Use Image Field Mapping'),
        '#description' => t('Select this checkbox only if you know for a fact that your RETS provider uses something other than the MLS ID for correlation of the Images to a listing ID.'),
        '#default_value' => variable_get("drealty_use_img_field_mapping_{$res}_{$conid}", 0),
    );
    $form['img_field_mapping'] = array(
        '#type' => 'select',
        '#title' => t("Image Field Mapping"),
        '#description' => t("Typically this is the MLS ID field. For Interrealty it is the sysid field, if in doubt of what to select here, ensure that the 'Use Image Field Mapping' checkbox is deselected."),
        '#options' => $options,
        '#default_value' => variable_get("drealty_img_field_mapping_{$res}_{$conid}", ""),
    );
    // Can we use an ImageField to store property images?
//    if (module_exists('imagefield')) {
      $form['use_img']['#default_value'] = variable_get("drealty_use_img_{$res}_{$conid}", FALSE);
      $form['use_img']['#description'] = t('Images will be automatically added to this field if you correlate a <em>Picture count</em> field above.');
//    } else {
//      $form['use_img']['#disabled'] = TRUE;
//      $form['img_field_mapping']['#disabled'] = TRUE;
//      $form['use_img_field_mapping']['#disabled'] = TRUE;
//      $form['use_img']['#description'] = t('You must install the <em>ImageField</em> module to use this feature.');
//    }
  }

  $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save configuration'),
  );

  return $form;
}

function drealty_configure_resource_correlate_validate($form, $form_state) {

  $dr = new drealtyResources();

  $corrs = array();
  $conid = $form_state['values']['conid'];
  $resource = $form_state['values']['resource'];
  $mappings = $dr->GetResourceMappings($conid);
  $res = $mappings["drealty_$resource"]->resource;

  $fields = $dr->FetchActiveFields($conid, $res);

  foreach ($fields as $field) {
    if ($form_state['values']['fields'][$field->systemname]['correlation'] !== 'none') {
      if (in_array($form_state['values']['fields'][$field->systemname]['correlation'], $corrs)) {
        form_set_error($form_state['values']['fields'][$field->systemname]['correlation'], t('%corr is correlated to more than one field.',
            array('%corr' => $form_state['values']['fields'][$field->systemname]['correlation'])));
      } else {
        $corrs[] = $form_state['values']['fields'][$field->systemname]['correlation'];
      }
    }
  }

  switch (strtolower($res)) {
    // Check for lack of required values
    case 'property':
      if (!in_array('mls_id', $corrs)) {
        form_set_error('_mls_corr', t('A correlation to <em>MLS ID number</em> is required.'));
      }
      if (!in_array('sale_status', $corrs)) {
        form_set_error('_sale_corr', t('A correlation to <em>Sale status</em> is required.'));
      }
      if (!in_array('agent_id', $corrs)) {
        form_set_error('_agent_id_corr', t('A correlation to <em>Agent ID</em> is required.'));
      }
      if (!in_array('office_id', $corrs)) {
        form_set_error('_office_id_corr', t('A correlation to <em>Office ID</em> is required.'));
      }
      break;
    case 'agent':
      if (!in_array('agent_id', $corrs)) {
        form_set_error('_agent_id_corr', t('A correlation to <em>Agent ID</em> is required.'));
      }
      if (!in_array('office_id', $corrs)) {
        form_set_error('_office_id_corr', t('A correlation to <em>Office ID</em> is required.'));
      }
      if (!in_array('agent_type', $corrs)) {
        form_set_error('_agent_type_corr', t('A correlation to <em>Agent Type</em> is required.'));
      }
      if (!in_array('agent_first_name', $corrs)) {
        form_set_error('_agent_first_name_corr', t('A correlation to <em>Agent First Name</em> is required.'));
      }
      if (!in_array('agent_last_name', $corrs)) {
        form_set_error('_agent_last_name_corr', t('A correlation to <em>Agent Last Name</em> is required.'));
      }
      break;
    case 'office':
      if (!in_array('office_type', $corrs)) {
        form_set_error('_office_type_corr', t('A correlation to <em>Office Type</em> is required.'));
      }
      if (!in_array('office_id', $corrs)) {
        form_set_error('_office_id_corr', t('A correlation to <em>Office ID</em> is required.'));
      }
      if (!in_array('office_name', $corrs)) {
        form_set_error('_office_name_corr', t('A correlation to <em>Office Name</em> is required.'));
      }
      break;
    case 'open_house':
      if (!in_array('open_house_id', $corrs)) {
        form_set_error('_open_house_id_corr', t('A correlation to <em>Open House ID</em> is required.'));
      }
      if (!in_array('office_id', $corrs)) {
        form_set_error('_office_id_corr', t('A correlation to <em>Office ID</em> is required.'));
      }
      if (!in_array('agent_id', $corrs)) {
        form_set_error('_agent_id_corr', t('A correlation to <em>Agent ID</em> is required.'));
      }
      if (!in_array('mls_id', $corrs)) {
        form_set_error('_mls_corr', t('A correlation to <em>MLS ID number</em> is required.'));
      }
      if (!in_array('sale_status', $corrs)) {
        form_set_error('_sale_corr', t('A correlation to <em>Sale status</em> is required.'));
      }
      break;
  }
}

function drealty_configure_resource_correlate_submit($form, &$form_state) {

$dr = new drealtyResources();

  $conid = $form_state['values']['conid'];

  $resource = $form_state['values']['resource'];
  $mappings = $dr->GetResourceMappings($conid);
  $res = $mappings["drealty_$resource"]->resource;
  $fields = $dr->FetchActiveFields($conid, $res);

  foreach ($fields as $field) {
    if ($form_state['values']['fields'][$field->systemname]['correlation'] === 'none') {
      $form_state['values']['fields'][$field->systemname]['correlation'] = '';
    }
    $record = array(
        'fid' => $field->fid,
        'correlation' => $form_state['values']['fields'][$field->systemname]['correlation'],
    );
    drupal_write_record('drealty_fields', $record, 'fid');
  }

  

  
  $use_img = variable_get("drealty_use_img_{$res}_{$conid}", FALSE);
  if($form_state['values']['use_img'] && !$use_img){
    variable_set("drealty_use_img_{$res}_{$conid}", TRUE);
    variable_set("drealty_use_img_field_mapping__{$res}_{$conid}", TRUE);
    variable_set("drealty_img_field_mapping_{$res}_{$conid}", $form_state['values']['img_field_mapping']);
  } elseif(!$form_state['values']['use_img'] && $use_img) {
    variable_set("drealty_use_img_{$res}_{$conid}", FALSE);
    variable_set("drealty_use_img_field_mapping__{$res}_{$conid}", FALSE);
    variable_del("drealty_img_field_mapping_{$res}_{$conid}");
  }

  $use_loc = variable_get("drealty_use_loc_{$res}_{$conid}", FALSE);

  drupal_set_message(t('The configuration options have been saved.'));

  // Clear caches
  cache_clear_all("drealty_fields_{$res}_{$conid}", 'cache');
  cache_clear_all("drealty_fields_active_{$res}_{$conid}", 'cache');
  cache_clear_all("drealty_fields_solr_result", 'cache');

}


//function drealty_configure_resource_limits($form, &$form_state, $conid = NULL, $resource = NULL) {
//
//  $dr = new drealtyResources();
//
//  // grab the mappings and see if we have one already
//  $mappings = $dr->GetResourceMappings($conid);
//  if (!isset($mappings["drealty_$resource"])) {
//    drupal_set_message(t('You must configure the @resource Resource type first.', array("@resource" => ucwords($resource))), 'error');
//    drupal_goto("admin/drealty/connection/resource/$conid/$resource");
//  }
//  $res = $mappings["drealty_$resource"]->resource;
//  $resources = $dr->FetchResources($conid);
//
//  $form['conid'] = array('#type' => 'value', '#value' => $conid);
//  $form['resource'] = array('#type' => 'value', '#value' => $resource);
//
//  $fields = $dr->FetchActiveFields($conid, $res);
//
//  $stat_field = FALSE;
//
//  foreach ($fields as $field) {
//
//    switch ($resource) {
//      case 'property':
//        if ($field->correlation === 'sale_status') {
//          $stat_field = $field;
//          $status = "Sale Status";
//        }
//        break;
//      case 'office':
//        if ($field->correlation === 'office_type') {
//          $stat_field = $field;
//          $status = "Office Type";
//        }
//        break;
//      case 'agent':
//        if ($field->correlation === 'agent_type') {
//          $stat_field = $field;
//          $status = "Agent Type";
//        }
//        break;
//      case 'open_house':
//        if ($field->correlation === 'sale_status') {
//          $stat_field = $field;
//          $status = "Sale Status";
//        }
//        break;
//    }
//    continue;
//  }
//
//  if ($stat_field === FALSE) {
//    drupal_set_message(t('You must correlate a @status field first.', array('@status' => $status)), 'error');
//    drupal_goto("admin/config/drealty/$conid/$resource/correlation");
//  }
//
//  $is_active = '';
//  $opts = array();
//
//  $lookup_table = array_shift(unserialize($stat_field->lookupname));
//
//  $dc = new drealtyConnection();
//
//  if ($dc->connect($conid)) {
//    $values = $dc->get_phrets()->GetLookupValues($res, $lookup_table);
//    foreach ($values as $val) {
//      $opts[$val['Value']] = check_plain($val['LongValue']);
//    }
//  }
//
//
//  $form['status'] = array(
//      '#type' => 'select',
//      '#title' => t($status),
//      '#required' => TRUE,
//      '#options' => $opts,
//      '#default_value' => explode(',', $resources[$res]->selection_values),
//      '#description' => t('Select the appropriate values to limit the search query.'),
//      '#multiple' => TRUE,
//  );
//
//  $form['status_override'] = array(
//      '#type' => 'checkbox',
//      '#title' => t('Override !s with the following query.', array('!s' => $status)),
//      '#default_value' => variable_get("drealty_override_status_query_{$resource}_{$conid}", FALSE),
//  );
//
//  $form['status_override_query'] = array(
//      '#type' => 'textfield',
//      '#title' => t('Query to use'),
//      '#default_value' => variable_get("drealty_override_status_query_text_{$resource}_{$conid}", ''),
//  );
//
//  $form['chunk'] = array(
//      '#title' => t('Record chunk size'),
//      '#type' => 'select',
//      '#options' => drupal_map_assoc(array(250, 500, 1000, 1500, 2000, 3000)) + array('0' => t('Unlimited')),
//      '#default_value' => $resources[$res]->chunk_size,
//      '#description' => t('When processing incoming records, the system will work with data structures containing this many records simultaneously,
//      and cache the rest for later. If updating listings is often failing due to out of memory errors and increasing PHP&rsquo;s memory allocation is not an option,
//      try decreasing this value. However, decreasing this value increases the amount of time it will take to update listings. The RETS server may override this value (MAXROWS).'),
//  );
//
//  $form['submit'] = array(
//      '#type' => 'submit',
//      '#value' => t('Save Configuration'),
//  );
//
//  return $form;
//}
//
//function drealty_configure_resource_limits_submit($form, &$form_state) {
//  $conid = $form_state['values']['conid'];
//  $resource = $form_state['values']['resource'];
// $dr = new drealtyResources();
//
//  // grab the mappings and see if we have one already
//  $mappings = $dr->GetResourceMappings($conid);
//
//  $res = $mappings["drealty_$resource"]->resource;
//
// $resources = $dr->FetchResources($conid);
//
//  $record = $resources[$res];
//  $record->selection_values = implode(',', $form_state['values']['status']);
//  $record->chunk_size = $form_state['values']['chunk'];
//
//  drupal_write_record('drealty_resources', $record, array('rid'));
//
//  cache_clear_all("drealty_resources_$conid", 'cache');
//  drupal_set_message(t("The configuration options have been saved."));
//  // set a variable that can be check to see if a particular 'type' has been configured.
//  variable_set("drealty_{$resource}_configured", TRUE);
//  variable_set("drealty_override_status_query_text_{$resource}_{$conid}", $form_state['values']['status_override_query']);
//  variable_set("drealty_override_status_query_{$resource}_{$conid}", $form_state['values']['status_override']);
//}

/**
 *  THEME FUNCTIONS
 *
 * */

function theme_drealty_fmanage_form(&$variables) {
  $header = array('Label', 'Classes', 'Correlation');
  if (module_exists('apachesolr')) {
    $header[] = 'Solr Indexed?';
    $header[] = 'Solr Result?';
  }

  $rows = array();
  $form = $variables['form'];
  foreach (element_children($form) as $key) {
    $row = array();
    $row[] = array('data' => drupal_render($form[$key]['label']));
    $row[] = array('data' => drupal_render($form[$key]['classes']));
    $row[] = array('data' => drupal_render($form[$key]['correlation']));
    if (module_exists('apachesolr')) {
      $row[] = array('data' => drupal_render($form[$key]['search_indexed']));
      $row[] = array('data' => drupal_render($form[$key]['search_returned']));
    }
    $rows[] = $row;
  }
  $output = theme('table', array('header' => $header, 'rows' => $rows)); 
  return $output;
}

function theme_drealty_fields_alt_form(&$form) {
  $header = array('Import this Field', 'Field Name', 'Field Label', 'Map to Existing Field');
  $rows = array();
  foreach (element_children($form) as $key) {
    $row = array();
    $row[] = array('data' => drupal_render($form[$key]['display']), 'style' => 'width: 40px;');
    $row[] = array('data' => drupal_render($form[$key]['field_name']), 'style' => 'width: 40px;');
    $row[] = array('data' => drupal_render($form[$key]['field_label']));
    $row[] = array('data' => drupal_render($form[$key]['field_map']));
    $rows[] = $row;
  }
  $output = theme('table', array('header' => $header, 'rows' => $rows));

  return $output;
}


