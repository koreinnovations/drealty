<?php

/**
 * @file drealty.admin.inc
 */

/*
 * Theme the base menu so that when you click on the main link it outputs the sublinks w/ their descriptions
 * on a single page.
*/
function drealty_base_admin_menu() {
  $item = menu_get_item('admin/drealty');
  $content = system_admin_menu_block($item);
  $output = theme('admin_block_content', $content);
  return $output;
}


function drealty_connection_list_form() {
  // grab the connections from the db
  $result = db_query("SELECT * FROM {drealty_connections}");
  $connections = array();
  $output = "";
  while ($con = db_fetch_array($result)) {
    $connections[$con['conid']] = $con;
  }

  if (!empty($connections)) {
    $rows = array();
    $header = array('Connection ID', 'Connection Name', 'Status', 'Operations');
    foreach ($connections as $key => $value) {
      $operation = l("Edit", "admin/drealty/connection/edit/{$value['conid']}") . ' | ' . l("Delete", "admin/drealty/connection/delete/{$value['conid']}");
      if ($value['active']) {
        $operation .= ' | ' . l("Deactivate", "admin/drealty/connection/deactivate/{$value['conid']}");
        $operation .= ' | ' . l("Export", "admin/drealty/connection/export/{$value['conid']}");
        $operation .= ' | ' . l("Data Management", "admin/drealty/connection/manual_update/{$value['conid']}");
        $operation .= ' <br /> ' . "Configure: " . l("Properties", "admin/drealty/connection/resource/{$value['conid']}/property");
        $operation .= ' | ' . l("Open Houses", "admin/drealty/connection/resource/{$value['conid']}/open_house");
        $operation .= ' | ' . l("Offices", "admin/drealty/connection/resource/{$value['conid']}/office");
        $operation .= ' | ' . l("Agents", "admin/drealty/connection/resource/{$value['conid']}/agent");
      }
      else {
        $operation .= ' | ' . l("Activate", "admin/drealty/connection/activate/{$value['conid']}");
      }
      $status = $value['active']?'Active':'Inactive';
      $rows[] = array($key, $value['name'], $status, $operation);
    }
    $output = theme('table', $header, $rows);
  }
  else {
    $output = "<div>There are no Connections setup. Please setup a new Connection.</div>";
  }

  $output .= "<br /><div>";
  $output .= l("Add Connection", "admin/drealty/connection/add");
  $output .= " | ";
  $output .= l("Import Connection", "admin/drealty/connection/import");
  $output .= "</div>";

  return $output;
}
/**
 * Menu callback -- ask for confirmation of connection deletion
 */
function drealty_connection_delete_confirm(&$form_state, $conid) {

  $connection = db_fetch_array(db_query("SELECT * FROM {drealty_connections} WHERE conid = %d", $conid));

  $form['conid'] = array(
    '#type' => 'value',
    '#value' => $conid,
  );
  $form['#redirect'] = 'admin/drealty/connection';

  return confirm_form($form,
    t('Are you sure you want to delete %title?', array('%title' => $connection['name'])),
    isset($_GET['destination']) ? $_GET['destination'] : 'admin/drealty/connection',
    t('This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}
/*
 *  Implementation of hook_form_submit() -- Deletes a Connection
*/
function drealty_connection_delete_confirm_submit($form, &$form_state) {
  // grab the connections so we can print out the connection name when we delete it
  $connections = drealty_connections_fetch();
  $conid = $form_state['values']['conid'];
  // grab the mappings. need them to itterate through to remove any cck fields
  $mappings = drealty_resource_mappings($conid);
  /**
   *  Since we are passing FALSE to rebuild on content_field_instance_delete()
   *  we need to keep track of wether we need to do it at the end.
   */
  $rebuild = FALSE;
  module_load_include('inc', 'content', 'includes/content.crud');
  foreach ($mappings as $mapping) {
    // delete any cck fields used by this connection
    $active_fields = drealty_fields_active_fetch($conid, $mapping['resource']);
    foreach ($active_fields as $field) {
      content_field_instance_delete("dr_{$field['cck_field_name']}", $mapping['node_type'], FALSE);
      $rebuild = TRUE;
    }
  }
  // remove fields
  if (db_query("DELETE FROM {drealty_fields} WHERE conid = %d", $conid)) {
    drupal_set_message(t("Deleted Fields."));
  }
  //remove lookups
  if (db_query("DELETE FROM {drealty_lookups} WHERE conid = %d", $conid)) {
    drupal_set_message(t("Deleted Lookups."));
  }
  // remove classes
  if (db_query("DELETE FROM {drealty_classes} WHERE conid = %d", $conid)) {
    drupal_set_message(t("Deleted Classes."));
  }
  // remove resources
  if (db_query("DELETE FROM {drealty_resources} WHERE conid = %d", $conid)) {
    drupal_set_message(t("Deleted Resources."));
  }
  // remove mappings
  if (db_query("DELETE FROM {drealty_resource_mappings} WHERE conid = %d", $conid)) {
    drupal_set_message(t("Deleted Mappings."));
  }
  // finally remove the connection
  if (db_query("DELETE FROM {drealty_connections} WHERE conid = %d", $conid)) {
    watchdog('drealty', "Deleted Connection: @conname", array('@conname' => $connections[$conid]['name']));
    drupal_set_message(t("Deleted Connection: @conname", array('@conname' => $connections[$conid]['name'])));
  }
  else {
    drupal_set_message(t("Failed to deleted Connection: @conname", array('@conname' => $connections[$conid]['name'])), WATCHDOG_ERROR);
  }

  // remove any global variables we used.

  // check to see if we need to rebuild
  if ($rebuild) {
    content_clear_type_cache(TRUE);
    menu_rebuild();
  }
  // dump the cache
  cache_clear_all('drealty_', 'cache', TRUE);
}
/*
 *  Returns the Connection add/edit form
*/
function drealty_connection_setup_form(&$form_state, $conid = NULL) {

  if (isset($conid)) {
    $value = db_fetch_array(db_query("SELECT * FROM {drealty_connections} WHERE conid = %d", $conid));
  }

  $form['#redirect'] = '/admin/drealty/connection';
  $form['connection'] = array('#type' => 'fieldset', '#title' => t('Connection Information'));
  $form['connection']['connection_id'] = array('#type' => 'value', '#value' => $conid);
  $form['connection']['connection_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Connection Name'),
    '#description' => t('Name to identify the connection in the administration pages.'),
    '#required' => TRUE,
    '#size' => 80,
    '#default_value' => isset($value['name'])?$value['name']:'',
  );
  $form['connection']['login_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Login URL'),
    '#default_value' => isset($value['login_url'])?$value['login_url']:'',
    '#required' => TRUE,
    '#size' => 80,
  );
  $form['connection']['connection_username'] = array(
    '#type' => 'textfield',
    '#title' => t('Username'),
    '#default_value' => isset($value['username'])?$value['username']:'',
    '#size' => 40,
    '#required' => TRUE,
  );
  $form['connection']['connection_password'] = array(
    '#type' => 'textfield',
    '#title' => t('Password'),
    '#default_value' => isset($value['password'])?$value['password']:'',
    '#size' => 40,
    '#required' => TRUE,
  );

  $form['connection']['user_agent'] = array(
    '#type' => 'textfield',
    '#title' => t('User Agent String'),
    '#description' => t('A User Agent String.'),
    '#required' => TRUE,
    '#size' => 80,
    '#default_value' => isset($value['ua_string'])?$value['ua_string']:'REALESTATEPRO/1.0',
  );
  $rets_versions = array('1.0' => '1.0', '1.5' => '1.5', '1.7' => '1.7', '1.7.2' => '1.7.2');
  $form['connection']['connection_version'] = array(
    '#type' => 'select',
    '#title' => t('RETS Version'),
    '#options' => $rets_versions,
    '#required' => TRUE,
    '#default_value' => isset($value['version'])?$value['version']:'1.5',
  );
  $form['connection']['force_basic_auth'] = array(
    '#type' => 'radios',
    '#title' => t('Force Basic Authentication'),
    '#options' => array(TRUE => 'yes', FALSE => 'no'),
    '#required' => TRUE,
    '#default_value' => isset($value['force_basic_auth'])?$value['force_basic_auth']:0,
  );
  $form['connection']['use_compression'] = array(
    '#type' => 'radios',
    '#title' => t('Use Compression'),
    '#options' => array(TRUE => 'yes', FALSE => 'no'),
    '#required' => TRUE,
    '#default_value' => isset($value['use_compression'])?$value['use_compression']:0,
  );
  $form['submit'] = array('#type' => 'submit', '#value' => t('Save'));
  return $form;
}
/*
 * Implementation of hook_form_validate() -- validates teh Connection Setup Form
*/
function drealty_connection_setup_form_validate($form, &$form_state) {

}
/*
 * Implementation of hook_form_submit() -- insert/update connection
*/
function drealty_connection_setup_form_submit($form, &$form_state) {

  $update = isset($form_state['values']['connection_id'])?array('conid'):array();
  $function = isset($form_state['values']['connection_id'])?'updating':'creating';

  $connection = array(
    'conid' => isset($form_state['values']['connection_id'])?$form_state['values']['connection_id']:NULL,
    'name' => $form_state['values']['connection_name'],
    'login_url' => $form_state['values']['login_url'],
    'username' => $form_state['values']['connection_username'],
    'password' => $form_state['values']['connection_password'],
    'ua_string' => $form_state['values']['user_agent'],
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
  }
  else {
    $message = t('dRealty IDX has encountered a problem while @function %connection_name. Please contact the system administrator if this continues.',
      array('@function' => $function, '%connection_name' => $form_state['values']['connection_name']));
    $severity = 'error';
  }

  cache_clear_all('drealty_connections', 'cache');
  watchdog('drealty', $message);
  drupal_set_message($message, $severity);
}

function drealty_activate_connection(&$form_state, $conid = NULL) {

  if (!isset($conid)) {
    drupal_set_message(t("Must have a connection selected."), 'error');
  }
  else {
    $connection = db_fetch_array(db_query("SELECT * FROM {drealty_connections} WHERE conid = %d", $conid));
    include_once "phRets/phRets.php";
    $ph_rets = new phRets();
    if (drealty_rets_connect($ph_rets, $conid)) {

      $server = $ph_rets->GetServerInformation();
      $resources = $ph_rets->GetMetadataResources();
      $login_url = $ph_rets->GetLoginURL();
      $server_software = $ph_rets->GetServerSoftware();
      $rets_version = $ph_rets->GetServerVersion();
      $caps = $ph_rets->GetAllTransactions();
      $capabilities = '';
      foreach ($caps as $cap) {
        $capabilities .= $cap . ', ';
      }
      $supported_caps = substr($capabilities, 0, strlen($capabilities) -2);
      $auth_supported = $ph_rets->CheckAuthSupport('digest')?"Digest Authentication is Supported":"(unknown - assuming Basic)";



      $form['server_info'] = array('#type' => 'fieldset', '#title' => 'Server Information');
      $form['server_info']['properties'] = array(
        '#type' => 'markup',
        '#value' => "<table border='0' cellpadding='1' cellspacing='1' width='100%'>\n".
          " <tr><td width='25%' valign='top'><strong>RETS Server:</strong></td><td width='75%' valign='top'>{$server['SystemDescription']}</td></tr>\n".
          " <tr><td width='25%' valign='top'><strong>RETS System ID:</strong></td><td width='75%' valign='top'>{$server['SystemID']}</td></tr>\n".
          " <tr><td width='25%' valign='top'><strong>Login URL:</strong></td><td width='75%' valign='top'>$login_url</td></tr>\n".
          " <tr><td width='25%' valign='top'><strong>RETS Version:</strong></td><td width='75%' valign='top'>{$rets_version}</td></tr>\n".
          " <tr><td width='25%' valign='top'><strong>Server Software:</strong></td><td width='75%' valign='top'>{$server_software}</td></tr>\n".
          " <tr><td width='25%' valign='top'><strong>Authentication Supported:</strong></td><td width='75%' valign='top'>{$auth_supported}</td></tr>\n".
          " <tr><td width='25%' valign='top'><strong>Transactions Supported:</strong></td><td width='75%' valign='top'>{$supported_caps}</td></tr>\n".
          "</table>"
      );
      $form['#redirect'] = 'admin/drealty/connection';
      $form['conid'] = array('#type' => 'value', '#value' => $conid);
      $form['activate'] = array(
        '#type' => 'submit',
        '#value' => t('Activate'),
      );
      $form['edit'] = array(
        '#type' => 'button',
        '#value' => t('Edit'),
      );
    }


  }
  return $form;
}
/*
 * Implementation of hook_form_submit() -- Activates a connection
*/
function drealty_activate_connection_submit($form, &$form_state) {

  $connection = db_fetch_array(db_query("SELECT * FROM {drealty_connections} WHERE conid = %d", $form_state['values']['conid']));
  $connection['active'] = 1;
  drupal_write_record('drealty_connections', $connection, array('conid'));

  //go ahead and grab all the lookup values and store them
  watchdog('drealty', "Activated Connection: @conname", array('@conname' => $connection['name']));
  drupal_set_message(t("Activated Connection: @conname", array('@conname' => $connection['name'])));

}
/*
 * Menu Callback -- Deactivates a connection
*/
function drealty_deactivate_connection($conid = NULL) {
  if (isset($conid)) {
    $connection = db_fetch_array(db_query("SELECT * FROM {drealty_connections} WHERE conid = %d", $conid));
    $connection['active'] = 0;
    drupal_write_record('drealty_connections', $connection, array('conid'));

    watchdog('drealty', "Deactivated Connection: @conname", array('@conname' => $connection['name']));
    drupal_set_message(t("Deactivated Connection: @conname", array('@conname' => $connection['name'])));
    drupal_goto('admin/drealty/connection');
  }
  else {
    drupal_set_message(t("Error deactivating connection."), 'error');
    drupal_goto('admin/drealty/connection');
  }
}


function drealty_configure_property($form_state, $conid = NULL, $resource = NULL) {

  if (!isset($conid)) {
    drupal_set_message(t("Must have a connection slected."), 'error');
    drupal_goto('/admin/drealty/connection');
  }
// grab the mappings and see if we have one already
  $mappings = drealty_resource_mappings($conid);

  $results = drealty_resources_fetch($conid);
  $resources = array();
  foreach ($results as $res) {
    $resources[$res['systemName']] = $res['systemName'];
  }

  $form['mid'] = array('#type' => 'value', '#value' => isset($mappings["drealty_$resource"])?$mappings["drealty_$resource"]['mid']:NULL);
  $form['conid'] = array('#type' => 'value', '#value' => $conid);
  $form['resource'] = array('#type' => 'value', '#value' => $resource);
  $form['prop_resource'] = array(
    '#type' => 'select',
    '#title' => t('@resource Resource', array('@resource' => ucwords(str_replace("_", " ", $resource)))),
    '#default_value' => isset($mappings["drealty_$resource"])?$mappings["drealty_$resource"]['resource']:NULL,
    '#options' => $resources,

  );
  $form['select_prop_resource'] = array(
    '#type' => 'button',
    '#value' => t('Select'),
    '#submit' => array('drealty_connection_configure'),
    '#suffix' => '<br /><br />',
  );

  if (isset($form_state['post']['prop_resource']) || isset($mappings["drealty_$resource"])) {
    if (isset($form_state['post']['prop_resource'])) {
      $results = drealty_classes_fetch($conid, $form_state['post']['prop_resource']);
    }
    else {
      $results = drealty_classes_fetch($conid, $mappings["drealty_$resource"]['resource']);
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
      $form['classes'][$key]['cname'] = array('#value' => $class['systemName']);
      $form['classes'][$key]['sname'] = array('#value' => "{$class['standardName']} {$class['description']}");

      $form['classes'][$key]['enabled'] = array('#type' => 'checkbox', '#default_value' => $class['enabled'], '#attributes' => $class['enabled']?array('checked' => 'checked'):NULL);

      $form['classes'][$key]['lifetime'] = array('#type' => 'select', '#options' => $life_opts, '#default_value' => isset($class['lifetime'])?$class['lifetime']:28000, );
      $form['classes'][$key]['rid'] = array('#type' => 'value', '#value' => $class['rid']);
      $form['classes'][$key]['conid'] = array('#type' => 'value', '#value' => $class['conid']);
      $form['classes'][$key]['cid'] = array('#type' => 'value', '#value' => $class['cid']);
      $form['classes'][$key]['classname'] = array('#type' => 'value', '#value' => $class['systemName']);
      $form['classes'][$key]['standardName'] = array('#type' => 'value', '#value' => $class['standardName']);
    }

    $form['save_changes'] = array(
      '#type' => 'submit',
      '#value' => t('Save Changes'),
    );

  }
  return $form;
}
/*
 * Implementation of hook_submit() -- Updates the Resource Type for Properties
 * Also saves the class configuation for enabled & lifetime
*/
function drealty_configure_property_submit($form, &$form_state) {


  $resource = $form_state['values']['resource'];

// update the resource mapping for this connection
  $mapping = array(
    'mid' => isset($form_state['values']['mid'])?$form_state['values']['mid']:NULL,
    'conid' => $form_state['values']['conid'],
    'resource' => $form_state['values']['prop_resource'],
    'node_type' => "drealty_$resource",
  );
// update or insert
  if (isset($mapping['mid'])) {
    drupal_write_record('drealty_resource_mappings', $mapping, array('mid'));
  }
  else {
    unset($mapping['mid']);
    drupal_write_record('drealty_resource_mappings', $mapping, array());
  }

  $select_opts = array();
  // update the classes
  foreach ($form_state['values']['classes'] as $class) {
    drupal_write_record('drealty_classes', $class, array('cid'));
    $select_opts[]= "{$class['classname']}|{$class['standardName']}";
  }
  module_load_include('inc', 'content', 'includes/content.crud');
  $class_field = content_field_instance_read(array('type_name' => "drealty_$resource", 'field_name' => "drealty_{$resource}_class"));
  $class_field[0]['allowed_values'] = implode("\n", $select_opts);
  content_field_instance_update($class_field[0]);

  $conid = $form_state['values']['conid'];

  //grab the lookups for this resource
  drealty_populate_all_lookups($conid, $form_state['values']['prop_resource']);

  // clear out the cached data
  cache_clear_all("drealty_classes_", 'cache', TRUE);
  cache_clear_all("drealty_mappings_$conid", 'cache');
  // set a succes message
  drupal_set_message(t('The Configuration options have been Saved.'));
}

function drealty_configure_property_fields(&$form_state, $conid = NULL, $resource = NULL) {
  if (!isset($conid)) {
    drupal_set_message(t("Must have a connection slected."), 'error');
    drupal_goto('/admin/drealty/connection');
  }

  // grab the groups so we can use them
  $groups = fieldgroup_groups("drealty_$resource", TRUE);

  // grab the mappings and see if we have one already
  $mappings = drealty_resource_mappings($conid);
  if (!isset($mappings["drealty_$resource"])) {
    drupal_set_message(t('You must configure the @resource Resource type first.', array("@resource" => ucwords($resource))), 'error');
    drupal_goto("admin/drealty/connection/resource/$conid/$resource");
  }
  $res = $mappings["drealty_$resource"]['resource'];
  $fields = drealty_fields_fetch($conid, $res);
  $connections = drealty_connections_fetch();
  $connection = $connections[$conid];


  $cck_field_opts = array();
  $none = array();
  $tmp_array = array();
  $none['none'] = 'none';
  foreach ($connections as $con) {
    $default_fields = drealty_fields_active_fetch($con['conid'], $res);
    $temp_field_opts = array();

    foreach ($default_fields as $cck_field) {
      $temp_field_opts[$cck_field['cck_field_name']] = $cck_field['cck_label'];
    }
    $tmp_array = array_merge($temp_field_opts, $tmp_array);
  }

  $cck_field_opts = array_merge($none, $tmp_array);


  $form['conid'] = array('#type' => 'value', '#value' => $conid);
  $form['resource'] = array('#type' => 'value', '#value' => $res);
  $form['node_type'] = array('#type' => 'value', '#value' => "drealty_$resource");
  $form['fields']['#tree'] = TRUE;


  if(count($connections)>1) {
    $form['fields']['#theme'] = 'drealty_fields_alt_form';
  }
  else {
    $form['fields']['#theme'] = 'drealty_fields_form';
  }

  foreach ($fields as $field) {
    $form['fields'][$field['systemName']]['display'] = array(
      '#type' => 'checkbox',
      '#title' => t('@sysname', array('@sysname' => $field['systemName'])),
      '#description' => t('@longname', array('@longname' => ucwords(strtolower($field['longName'])))),
      '#default_value' => $field['display'],
    );
    if (!$field['mapped_field']) {
      $form['fields'][$field['systemName']]['cck_field_name'] = array(
        '#type' => 'textfield',
        '#default_value' => $field['cck_field_name']?$field['cck_field_name']:strtolower(substr($field['systemName'], 0, 29)),
        '#field_prefix' => '<span dir="ltr">dr_',
        '#field_suffix' => '</span>',
        '#maxlength' => 29,
        '#size' => 29,
        '#description' => t('Field name (a-z, 0-9, _)'),
      );
      $form['fields'][$field['systemName']]['cck_label'] = array(
        '#type' => 'textfield',
        '#default_value' => $field['cck_label']?$field['cck_label']:ucwords(strtolower($field['longName'])),
        '#size' => 30,
        '#description' => t('&nbsp;')
      );
    }
    else {
      $form['fields'][$field['systemName']]['cck_field_name'] = array(
        '#type' => 'item',
        '#value' => $field['cck_field_name']?$field['cck_field_name']:strtolower(substr($field['systemName'], 0, 29)),
      );
      $form['fields'][$field['systemName']]['cck_label'] = array(
        '#type' => 'item',
        '#value' => $field['cck_label']?$field['cck_label']:$field['longName'],
      );
    }
    $form['fields'][$field['systemName']]['cck_map'] = array(
      '#type' => 'select',
      '#options' => $cck_field_opts,
      '#default_value' => $field['cck_field_name']?$field['cck_field_name']:'none',
      '#description' => t('&nbsp;'),
    );

  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save Configuration'),
    '#submit' => array('drealty_configure_property_fields_submit'),

  );
  return $form;
}
function drealty_configure_property_fields_validate($form, &$form_state) {

  $fields = $form_state['values']['fields'];
  $conid = $form_state['values']['conid'];
  $resource = $form_state['values']['resource'];
  $active_fields = drealty_fields_active_fetch($conid, $resource);


  module_load_include('inc', 'content', 'includes/content.crud');

  $cck_fields = content_field_instance_read(array(), TRUE);

  foreach ($fields as $key => $field) {
    if (!preg_match('!^[a-z0-9_]+$!', $field['cck_field_name']) && $field['display'] == 1) {
      form_set_error("fields][{$key}][cck_field_name", t('Add new field: the field name %field_name is invalid. The name must include only lowercase unaccentuated letters, numbers, and underscores.', array('%field_name' => $field['cck_field_name'])));
    }
    foreach ($cck_fields as $existing_field) {
      if ("dr_{$field['cck_field_name']}" == $existing_field['field_name'] && $field['display'] == 1 && !$active_fields[$key]['display']) {
        form_set_error("fields][{$key}][cck_field_name", t('Adding field: the field name: %field_name already exists.', array('%field_name' => $field['cck_field_name'])));
      }
    }
  }
}
function drealty_configure_property_fields_submit($form, &$form_state) {
  module_load_include('inc', 'content', 'includes/content.crud');
  $conid = $form_state['values']['conid'];
  $resource = $form_state['values']['resource'];
  $node_type = $form_state['values']['node_type'];
  $fields = drealty_fields_fetch($conid, $resource);

  $connections = drealty_connections_fetch();
  $connection = $connections[$conid];

  $default_fields = array();
  foreach ($connections as $con) {
    $tmp_fields = drealty_fields_active_fetch($con['conid'], $resource);
    $default_fields = array_merge($tmp_fields, $default_fields);
  }


  foreach ($fields as $id => $field) {

    if ($form_state['values']['fields'][$id]['display'] && !$field['display']) {
      //check to see if we need to create  a new field or map it to an existing one.
      if ($form_state['values']['fields'][$id]['cck_map'] === 'none') {
        $cck_field = array(
          'field_name' => 'dr_' . $form_state['values']['fields'][$id]['cck_field_name'],
          'type_name' => $node_type,
          'label' => $form_state['values']['fields'][$id]['cck_label'],
          'group' => "{$node_type}_group",
        );
        //type
        if ($field['interpretation'] === 'LookupMulti' || $field['interpretation'] === 'Lookup') {

          $values = array();
          $value_holder = array();
          $sort_by = array();
          $lookup_tables = unserialize($field['lookupName']);

          foreach ($lookup_tables as $key => $value) {
            $lookups = drealty_lookups_fetch($conid, $resource, $value, $key);
            $tmp_values = array();
            foreach ($lookups as $lup) {
              $plain = check_plain($lup['longValue']);
              $sort_by[] = $plain;
              $tmp_values["{$lup['longValue']}"] = "{$lup['longValue']}|{$lup['longValue']}";
            }
            // array_merge() should remove any duplicates by merging them together since the key is a string.
            $value_holder = array_merge($tmp_values, $value_holder);
          }

          sort($value_holder);
          $values = array_values($value_holder);

          $cck_field['type'] = 'text';
          $cck_field['max_length'] = 60; //$field['max_length'];
          $cck_field['widget_type'] = 'optionwidgets_select';
          $cck_field['allowed_values'] = implode("\n", $values);
          $cck_field['multiple'] = $field['interpretation'] == 'LookupMulti' ? 1 : 0;
        }
        else {
          if (in_array($field['dataType'], array('Boolean', 'Tiny', 'Small', 'Int', 'Long')) || ($field['dataType'] === 'Decimal' && $field['field_precision'] == 0)) {
            $cck_field['type'] = 'number_integer';
            $cck_field['widget_type'] = 'number';
          }
          elseif ($field['dataType'] === 'Decimal') {
            $cck_field['type'] = 'number_decimal';
            $cck_field['widget_type'] = 'number';
            // Note that RETS and this CCK field are using the term "precision"
            // differently
            $cck_field['precision'] = $field['max_length'];
            $cck_field['scale'] = $field['field_precision'];
          }
          elseif (in_array($field['dataType'], array('Date', 'DateTime'))) {
            $cck_field['type'] = 'datestamp';
            $cck_field['widget_type'] = 'date_text';
            $cck_field['widget_settings'] = array(
              'input_format' => 'm/d/Y - H:i:s',
            ); // @todo: Format this according to Date module defaults?
            $cck_field['year_range'] = '-0:+1';
            $cck_field['granularity'] = array(
              'year' => 'year',
              'month' => 'month',
              'day' => 'day',
              'hour' => 'hour',
              'minute' => 'minute',
              'second' => 'second',
            );
          }
          else {
            // Assume all others are Character
            $cck_field['type'] = 'text';
            $cck_field['max_length'] = $field['max_length'];
            $cck_field['widget_type'] = $field['max_length'] > 127 ? 'text_textarea' : 'text_textfield';
          }
        }


        content_field_instance_create($cck_field, FALSE);
        if (module_exists('fieldgroup')) {
          fieldgroup_update_fields($cck_field);
        }
        if (module_exists('content_permissions') && module_exists('permissions_api')) {
          $permissions_view = array("view {$cck_field['field_name']}");
          $permissions_edit = array("edit {$cck_field['field_name']}");
          $roles = user_roles();
          foreach ($roles as $role) {
            permissions_grant_permissions($role, $permissions_view);
            permissions_revoke_permissions($role, $permissions_edit);
          }
        }
        $record = array(
          'fid' => $field['fid'],
          'display' => 1,
          'cck_field_name' => $form_state['values']['fields'][$id]['cck_field_name'],
          'cck_label' => $form_state['values']['fields'][$id]['cck_label'],
          'mapped_field' => 0,
        );
        $status = "Creating";
      }
      else {
        //map this field to an existing cck field
        $map_field = $form_state['values']['fields'][$id]['cck_map'];
        $match = NULL;
        foreach ($default_fields as $item) {
          if ($item['cck_field_name'] === $map_field) {
            $match = $item;
            break;
          }
        }
        if (isset($match)) {
          $record = array(
            'fid' => $field['fid'],
            'display' => 1,
            'cck_field_name' => $match['cck_field_name'],
            'cck_label' => $match['cck_label'],
            'mapped_field' => 1,
          );
        }
        $status = "Mapping";
      }

      drupal_set_message(t('@status field %label (@id).', array('%label' => $cck_field['label'], '@id' => $id, '@status' => $status)));
      drupal_write_record('drealty_fields', $record, array('fid'));


    }
    // Else if the field is unchecked on the form but we ARE currently using it…
    elseif (!$form_state['values']['fields'][$id]['display'] && $field['display']) {

      if (!$field['mapped_field']) {
        $label = db_result(db_query('SELECT label FROM {content_node_field_instance} WHERE field_name = "%s" && type_name = "%s"', 'dr_' . $field['cck_field_name'], $node_type));
        drupal_set_message(t('Removing field %label (@id).', array('%label' => $label, '@id' => $id)));
        content_field_instance_delete('dr_' . $field['cck_field_name'], $node_type, FALSE);
        // If that field was the image count field, delete the image field too.
        if ($field['correlation'] === 'pic_count') {
          drupal_set_message(t('Removing <em>Images</em> field.'));
          content_field_instance_delete('dr_images', $node_type, FALSE);
        }
      }


      $record = array(
        'fid' => $field['fid'],
        'display' => 0,
        'mapped_field' => 0,
        'cck_field_name' => '',
        'cck_label' => '',
      );

      drupal_write_record('drealty_fields', $record, array('fid'));
    }
    elseif ($form_state['values']['fields'][$id]['display'] && $field['display'] && $field['mapped_field'] && $form_state['values']['fields'][$id]['cck_map'] === 'none') {
      $record = array(
        'fid' => $field['fid'],
        'display' => 0,
        'cck_field_name' => '',
        'cck_label' => '',
        'mapped_field' => 0,
      );
      drupal_set_message(t("Removing the Mapping from @field_name to @mapping_name.", array("@field_name" => $field['systemName'], "@mapping_name" => $field['cck_label'])));
      drupal_write_record('drealty_fields', $record, array('fid'));
    }
  }
  drupal_set_message(t('The configuration options have been saved.'));
  content_clear_type_cache(TRUE);
  menu_rebuild();
  cache_clear_all("drealty_fields_{$resource}_{$conid}", 'cache');
  cache_clear_all("drealty_fields_active_{$resource}_{$conid}", 'cache');
}

function drealty_configure_property_correlate(&$form_state, $conid = NULL, $resource = NULL) {
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
        'office_type' => t("Office Type"),
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
        'mls_id' => t("MLS ID number"),
        'sale_status' => t('Sale status'),
        'agent_id' => t('Agent ID'),
        'office_id' => t('Office ID'),
      );
      break;
  }
// grab the mappings and see if we have one already
  $mappings = drealty_resource_mappings($conid);
  if (!isset($mappings["drealty_$resource"])) {
    drupal_set_message(t('You must configure the @resource Resource type first.', array("@resource" => ucwords($resource))), 'error');
    drupal_goto("admin/drealty/connection/resource/$conid/$resource");
  }
  $res = $mappings["drealty_$resource"]['resource'];

  $form['conid'] = array('#type' => 'value', '#value' => $conid);
  $form['resource'] = array('#type' => 'value', '#value' => $resource);

  $fields = drealty_fields_active_fetch($conid, $res);

  $form['fields']['#tree'] = TRUE;
  $form['fields']['#theme'] = 'drealty_fmanage_form';

  foreach ($fields as $field) {
    $form['fields'][$field['systemName']]['label'] = array('#value' => t('@sysname (@longname)', array('@sysname' => $field['cck_field_name'], '@longname' => $field['cck_label'])));
    $form['fields'][$field['systemName']]['classes'] = array('#value' => implode(', ', array_map('check_plain', explode(', ', $field['classes']))));
    $form['fields'][$field['systemName']]['correlation'] = array('#type' => 'select', '#options' => $corrs, '#default_value' => $field['correlation'] === '' ? 'none' : $field['correlation']);
    if ( module_exists('apachesolr')) {
      $form['fields'][$field['systemName']]['search_indexed'] = array('#type' => 'checkbox', '#default_value' => $field['search_indexed']);
      $form['fields'][$field['systemName']]['search_returned'] = array('#type' => 'checkbox', '#default_value' => $field['search_returned']);
    }

  }


  if ($resource == 'property') {
    $form['use_img'] = array(
      '#type' => 'checkbox',
      '#title' => t('Add a CCK ImageField to property nodes to store property images'),
    );


    $connections = drealty_connections_fetch();

    if (count($connections) > 1) {

    } else if (count($connections) == 1) {
      $connection = array_shift($connections);
      $fields = drealty_fields_active_fetch($connection['conid'],'Property');
      $options = array();
      foreach ($fields as $field) {
        $options[$field['systemName']] = $field['systemName'];
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
    if (module_exists('imagefield')) {
      $form['use_img']['#default_value'] = variable_get("drealty_use_img_{$res}_{$conid}", FALSE);
      $form['use_img']['#description'] = t('Images will be automatically added to this field if you correlate a <em>Picture count</em> field above.');
    }
    else {
      $form['use_img']['#disabled'] = TRUE;
      $form['img_field_mapping']['#disabled'] = TRUE;
      $form['use_img_field_mapping']['#disabled'] = TRUE;
      $form['use_img']['#description'] = t('You must install the <em>ImageField</em> module to use this feature.');
    }
  }
  if ($resource == 'property' || $resource == 'office') {
    $form['use_loc'] = array(
      '#type' => 'checkbox',
      '#title' => t('Add a CCK Location field to property nodes to store address information'),
    );
    // Can we use a Location field to store property location info?
    if (module_exists('location_cck')) {
      $form['use_loc']['#default_value'] = variable_get("drealty_use_loc_{$res}_{$conid}", FALSE);
      $form['use_loc']['#description'] = t('Location data will be added to this field according to the <em>Location</em> fields correlated above. If your RETS provider uses several separate fields to store the street address, correlate each field in order (<em>Location: Street address 1</em>, <em>Location: Street address 2</em>, etc) and the values will be concatenated into a single string when stored.');
    }
    else {
      $form['use_loc']['#disabled'] = TRUE;
      $form['use_loc']['#description'] = t('You must install the <em>Location CCK</em> module to use this feature.');
    }
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  );



  return $form;
}

function drealty_configure_property_correlate_validate($form, $form_state) {
  $corrs = array();
  $conid = $form_state['values']['conid'];
  $resource = $form_state['values']['resource'];
  $mappings = drealty_resource_mappings($conid);
  $res = $mappings["drealty_$resource"]['resource'];

  $fields = drealty_fields_active_fetch($conid, $res);
  foreach ($fields as $field) {
    if ($form_state['values']['fields'][$field['systemName']]['correlation'] !== 'none') {
      if (in_array($form_state['values']['fields'][$field['systemName']]['correlation'], $corrs)) {
        form_set_error($form_state['values']['fields'][$field['systemName']]['correlation'], t('%corr is correlated to more than one field.',
          array('%corr' => $form_state['values']['fields'][$field['systemName']]['correlation'])));
      }
      else {
        $corrs[] = $form_state['values']['fields'][$field['systemName']]['correlation'];
      }
    }
  }

  switch ($res) {
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

function drealty_configure_property_correlate_submit($form, &$form_state) {

  module_load_include('inc', 'content', 'includes/content.crud');

  $conid = $form_state['values']['conid'];

  $resource = $form_state['values']['resource'];
  $mappings = drealty_resource_mappings($conid);
  $res = $mappings["drealty_$resource"]['resource'];
  $fields = drealty_fields_active_fetch($conid, $res);

  foreach ($fields as $field) {
    if ($form_state['values']['fields'][$field['systemName']]['correlation'] === 'none') {
      $form_state['values']['fields'][$field['systemName']]['correlation'] = '';
    }
//    if ($form_state['values']['fields'][$field['systemName']]['correlation'] !== $field['correlation']
//      || $form_state['values']['fields'][$field['systemName']]['search_indexed'] !=  $field['search_indexed']
//      || $form_state['values']['fields'][$field['systemName']]['search_returned'] !=  $field['search_returned']) {
    $record = array(
      'fid' => $field['fid'],
      'correlation' => $form_state['values']['fields'][$field['systemName']]['correlation'],
    );
    if (module_exists('apachesolr')) {
      $record['search_indexed'] = $form_state['values']['fields'][$field['systemName']]['search_indexed'];
      $record['search_returned'] = $form_state['values']['fields'][$field['systemName']]['search_returned'];
    }
    $cck_field = content_fields("dr_{$field['cck_field_name']}", "drealty_{$resource}");

    $values = array(
      NODE_BUILD_SEARCH_INDEX => array('exclude' => $form_state['values']['fields'][$field['systemName']]['search_indexed']?0:1),
    );

    $cck_field['display_settings'] = $values + $cck_field['display_settings'];
    content_field_instance_update($cck_field, FALSE);

    drupal_write_record('drealty_fields', $record, array('fid'));
//    }
  }

  $use_img = variable_get("drealty_use_img_{$res}_{$conid}", FALSE);
  if ($form_state['values']['use_img'] && !$use_img) {
    $img = array(
      'field_name' => 'dr_images',
      'type_name' => "drealty_$resource",
      'label' => t('Images'),
      'type' => 'filefield',
      'widget_type' => 'imagefield_widget',
      'file_extensions' => 'gif jpg jpeg png',
      'custom_alt' => FALSE,
      'custom_title' => FALSE,
      'group' => "{$resource}_group",
      'multiple' => TRUE,
      'weight' => -10,
    );

    drupal_set_message(t('Creating <em>Images</em> field.'));
    content_field_instance_create($img, FALSE);
    if (module_exists('fieldgroup')) {
      fieldgroup_update_fields($img);
    }
    if (module_exists('content_permissions') && module_exists('permissions_api')) {
      $permissions_view = array("view {$img['field_name']}");
      $permissions_edit = array("edit {$img['field_name']}");
      $roles = user_roles();
      foreach ($roles as $role) {
        permissions_grant_permissions($role, $permissions_view);
        permissions_revoke_permissions($role, $permissions_edit);
      }
    }
    variable_set("drealty_use_img_{$res}_{$conid}", TRUE);
    variable_set("drealty_use_img_field_mapping_{$res}_{$conid}", TRUE);
    variable_set("drealty_img_field_mapping_{$res}_{$conid}", $form_state['values']['img_field_mapping']);
  }
  elseif (!$form_state['values']['use_img'] && $use_img) {

    drupal_set_message(t('Removing <em>Images</em> field.'));
    content_field_instance_delete('dr_images', "drealty_$resource", FALSE);
    variable_set("drealty_use_img_{$res}_{$conid}", FALSE);
    variable_set("drealty_use_img_field_mapping_{$res}_{$conid}", FALSE);
    variable_del("drealty_img_field_mapping_{$res}_{$conid}");
  }

  $use_loc = variable_get("drealty_use_loc_{$res}_{$conid}", FALSE);
  if ($form_state['values']['use_loc'] && !$use_loc) {
    // Create a Location field and add it to the property content type
    $loc = drealty_get_loc_array("drealty_$resource");

    drupal_set_message(t('Creating <em>Location</em> field.'));
    content_field_instance_create($loc, FALSE);
    if (module_exists('fieldgroup')) {
      fieldgroup_update_fields($loc);
    }
    if (module_exists('content_permissions') && module_exists('permissions_api')) {
      $permissions_view = array("view {$loc['field_name']}");
      $permissions_edit = array("edit {$loc['field_name']}");
      $roles = user_roles();
      foreach ($roles as $role) {
        permissions_grant_permissions($role, $permissions_view);
        permissions_revoke_permissions($role, $permissions_edit);
      }
    }
    variable_set("drealty_use_loc_{$res}_{$conid}", TRUE);
  }
  elseif (!$form_state['values']['use_loc'] && $use_loc) {
    // Remove the Location field from the property content type
    drupal_set_message(t('Removing <em>Location</em> field.'));
    content_field_instance_delete('dr_loc', "drealty_$resource", FALSE);
    variable_set("drealty_use_loc_{$res}_{$conid}", FALSE);
  }
  drupal_set_message(t('The configuration options have been saved.'));

  // Clear caches and rebuild menu.
  content_clear_type_cache(TRUE);
  cache_clear_all("drealty_fields_{$res}_{$conid}", 'cache');
  cache_clear_all("drealty_fields_active_{$res}_{$conid}", 'cache');
  menu_rebuild();
}


function drealty_configure_property_limits(&$form_state, $conid = NULL, $resource = NULL) {
  // grab the mappings and see if we have one already
  $mappings = drealty_resource_mappings($conid);
  if (!isset($mappings["drealty_$resource"])) {
    drupal_set_message(t('You must configure the @resource Resource type first.', array("@resource" => ucwords($resource))), 'error');
    drupal_goto("admin/drealty/connection/resource/$conid/$resource");
  }
  $res = $mappings["drealty_$resource"]['resource'];
  $resources = drealty_resources_fetch($conid);

  $form['conid'] = array('#type' => 'value', '#value' => $conid);
  $form['resource'] = array('#type' => 'value', '#value' => $resource);

  $fields = drealty_fields_active_fetch($conid, $res);

  $stat_field = FALSE;

  foreach ($fields as $field) {

    switch ($resource) {
      case 'property':
        if ($field['correlation'] === 'sale_status') {
          $stat_field = $field;
          $status = "Sale Status";
        }
        break;
      case 'office':
        if ($field['correlation'] === 'office_type') {
          $stat_field = $field;
          $status = "Office Type";
        }
        break;
      case 'agent':
        if ($field['correlation'] === 'agent_type') {
          $stat_field = $field;
          $status = "Agent Type";
        }
        break;
      case 'open_house':
        if ($field['correlation'] === 'sale_status') {
          $stat_field = $field;
          $status = "Sale Status";
        }
        break;
    }
    continue;
  }

  if ($stat_field === FALSE) {
    drupal_set_message(t('You must correlate a @status field first.', array('@status' => $status)), 'error');
    drupal_goto("admin/drealty/$conid/$resource/correlation");
  }

  $is_active = '';
  $opts = array();

  $lookup_table = array_shift(unserialize($stat_field['lookupName']));

  $lookups = drealty_lookups_fetch($conid, $res, $lookup_table);
  foreach ($lookups as $look) {
    $opts[$look['value']] = check_plain($look['longValue']);
  }

  $form['status'] = array(
    '#type' => 'select',
    '#title' => t($status),
    '#required' => TRUE,
    '#options' => $opts,
    '#default_value' => explode(',', $resources[$res]['selection_values']),
    '#description' => t('Select the appropriate values to limit the search query.'),
    '#multiple' => TRUE,
  );

  $form['status_override'] = array(
    '#type' => 'checkbox',
    '#title' => t('Override !s with the following query.', array('!s' => $status)),
    '#default_value' => variable_get("drealty_override_status_query_{$resource}_{$conid}", FALSE),
  );

  $form['status_override_query'] = array(
    '#type' => 'textfield',
    '#title' => t('Query to use'),
    '#default_value' => variable_get("drealty_override_status_query_text_{$resource}_{$conid}", ''),
  );

  $form['chunk'] = array(
    '#title' => t('Record chunk size'),
    '#type' => 'select',
    '#options' => drupal_map_assoc(array(250, 500, 1000, 1500, 2000, 3000)) + array('0' => t('Unlimited')),
    '#default_value' => $resources[$res]['chunk_size'],
    '#description' => t('When processing incoming records, the system will work with data structures containing this many records simultaneously,
      and cache the rest for later. If updating listings is often failing due to out of memory errors and increasing PHP&rsquo;s memory allocation is not an option,
      try decreasing this value. However, decreasing this value increases the amount of time it will take to update listings. The RETS server may override this value (MAXROWS).'),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save Configuration'),
  );

  return $form;
}

function drealty_configure_property_limits_submit($form, &$form_state) {
  $conid = $form_state['values']['conid'];
  $resource = $form_state['values']['resource'];

  $mappings = drealty_resource_mappings($conid);
  $res = $mappings["drealty_$resource"]['resource'];

  $resources = drealty_resources_fetch($conid);

  $record = $resources[$res];
  $record['selection_values'] = implode(',', $form_state['values']['status']);
  $record['chunk_size'] = $form_state['values']['chunk'];

  drupal_write_record('drealty_resources', $record, array('rid'));

  cache_clear_all("drealty_resources_$conid", 'cache');
  drupal_set_message(t("The configuration options have been saved."));
  // set a variable that can be check to see if a particular 'type' has been configured.
  variable_set("drealty_{$resource}_configured", TRUE);
  variable_set("drealty_override_status_query_text_{$resource}_{$conid}", $form_state['values']['status_override_query']);
  variable_set("drealty_override_status_query_{$resource}_{$conid}", $form_state['values']['status_override']);

}

function theme_drealty_fmanage_form(&$form) {
  $header = array('Label', 'Classes', 'Correlation');
  if (module_exists('apachesolr')) {
    $header[] = 'Solr Indexed?';
    $header[] = 'Solr Result?';
  }

  $rows = array();
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
  $output = theme('table', $header, $rows);
  return $output;
}
function theme_drealty_fields_alt_form(&$form) {
  $header = array('Import this Field', 'CCK System Name', 'CCK Label', 'Map to Existing CCK Field');
  $rows = array();
  foreach (element_children($form) as $key) {
    $row = array();
    $row[] = array('data' => drupal_render($form[$key]['display']), 'style' => 'width: 40px;');
    $row[] = array('data' => drupal_render($form[$key]['cck_field_name']), 'style' => 'width: 40px;');
    $row[] = array('data' => drupal_render($form[$key]['cck_label']));
    $row[] = array('data' => drupal_render($form[$key]['cck_map']));
    $rows[] = $row;
  }
  $output = theme('table', $header, $rows);

  return $output;
}
function theme_drealty_fields_form(&$form) {
  $header = array('Import this Field', 'CCK System Name', 'CCK Label');
  $rows = array();
  foreach (element_children($form) as $key) {
    $row = array();
    $row[] = array('data' => drupal_render($form[$key]['display']), 'style' => 'width: 40px;');
    $row[] = array('data' => drupal_render($form[$key]['cck_field_name']), 'style' => 'width: 40px;');
    $row[] = array('data' => drupal_render($form[$key]['cck_label']));
    $rows[] = $row;
  }
  $output = theme('table', $header, $rows);

  return $output;

}

function theme_drealty_classes_form(&$form) {
  $header = array('System Name', 'Description', 'Enabled', 'Lifetime');
  $rows = array();
  foreach (element_children($form) as $key) {
    $row = array();
    $row[] = array('data' => drupal_render($form[$key]['cname']));
    $row[] = array('data' => drupal_render($form[$key]['sname']));
    $row[] = array('data' => drupal_render($form[$key]['enabled']));
    $row[] = array('data' => drupal_render($form[$key]['lifetime']));
    $rows[] = $row;
  }
  $output = theme('table', $header, $rows);
  //$output .= drupal_render($form);
  return $output;
}

function drealty_manual_update_form($form_state, $conid = NULL) {

  $connections = drealty_connections_fetch();
  $mappings = drealty_resource_mappings($conid);
  $resources = drealty_resources_fetch($conid);
  $form["connection_{$conid}"] = array('#type' => 'fieldset', '#title' => t($connections[$conid]['name']));


  foreach ($mappings as $mapping) {
    $classes = drealty_classes_fetch($conid, $mapping['resource']);
    foreach ($classes as $class) {
      if ($class['enabled']) {

        $count = db_result(db_query("SELECT COUNT(DISTINCT(nid)) FROM {content_type_{$mapping['node_type']}} WHERE {$mapping['node_type']}_class_value = '%s'", $class['systemName']));
        $form["connection_{$conid}"]["mapping_{$mapping['mid']}"]["update_{$class['systemName']}"] = array(
          '#type' => 'item',
          '#title' => t('@type - @visible (@class)', array('@type' => $mapping['resource'], '@visible' => $class['systemName'], '@class' => $class['standardName'])),
          '#description' => t('%count in database; last updated %update', array('%count' => $count, '%update' => $class['lastupdate'] ? format_date($class['lastupdate']) : t('(never)'))),
          '#value' => l(t('Update now'), "admin/drealty/{$conid}/{$mapping['resource']}/{$class['systemName']}/{$mapping['node_type']}/batch") . '&nbsp;|&nbsp;'.
            l(t('flush'), "admin/drealty/{$conid}/{$mapping['resource']}/{$class['systemName']}/{$mapping['node_type']}/flush"),
          '#weight' => $weight += 10,
        );

      }
    }
  }

  $form['flush_all'] = array(
    '#type' => 'item',
    '#title' => t('Flush all'),
    '#value' => l(t('Flush all'), "admin/drealty/{$conid}/flush/all"),
    '#weight' => $weight += 10,
  );

  $form['process_images'] = array(
    '#type' => 'item',
    '#title' => t('Process MLS Image Data'),
    '#description' => t('This will download all image data and replace the placeholders with the actual image data.'),
    '#value' => l(t('Process Images'), "admin/drealy/{$conid}/images/process"),
    '#weight' => $weight += 10,
  );
  return $form;
}

function drealty_process_images($form, $conid = NULL) {

  $connections = drealty_connections_fetch();
  $form = array();
  $form['conid'] = array('#type' => 'value', '#value' => $conid);

  $question =t('Are you sure you want to process all the MLS Images for the %connection connection?', array('%connection' => $connections[$conid]['name']));
  $path = "admin/drealty/connection/{$conid}/process_images";
  $description = t('The MLS image data will be retrieved from this server. This operation may take a significant amount of time dependent on the number of images to process; please be patient.');

  return confirm_form($form, $question, $path, $description, t('Process Images'));
}

function drealty_process_images_submit($form, &$form_state) {

  $connections = drealty_connections_fetch();
  $conid = $form_state['values']['conid'];

  $result = db_query("SELECT nid FROM {content_type_drealty_property} WHERE drealty_property_conid_value = %d", $conid);

  $batch_title = t('Processing all MLS Image Data for connection: @connection. ', array('@connection' => $connections[$conid]['name']));

  $nids = array();
  while ($nid = db_fetch_object($result)) {
    $nids[] = $nid->nid;
  }

  batch_set(array(
    'title' => $batch_title,
    'operations' => array(
      array('drealty_process_images_batch', array($nids)),
    ),
    'finished' => 'drealty_process_images_batch_finished',
    'file' => drupal_get_path('module', 'drealty') . '/drealty.batch.inc',
  ));
}

/**
 * Confirm flushing of listings. drupal_get_form() callback.
 */
function drealty_flush($form, $conid = NULL, $resoruce = NULL, $class = NULL, $type = NULL) {

  $form = array();
  $form['conid'] = array('#type' => 'value', '#value' => $conid);
  $form['resource'] = array('#type' => 'value', '#value' => $resource);
  $form['class'] = array('#type' => 'value', '#value' => $class);
  $form['node_type'] = array('#type' => 'value', '#value' => $type);

  $question =t('Are you sure you want to delete all property listings in the %class class?', array('%class' => $class));
  $path = "admin/drealty/connection/manual_update/{$conid}";
  $description = t('The listings will be deleted from this server, but the listings on the RETS server will not be modified and may be re-fetched if they still exist.
      This operation may take a significant amount of time if there are many listings to delete; please be patient.');

  return confirm_form($form, $question, $path, $description, t('Delete'));
}

/**
 * Submission function for drealty_flush().
 *
 */
function drealty_flush_submit($form, &$form_state) {

  $connections = drealty_connections_fetch();
  $conid = $form_state['values']['conid'];
  $node_type = $form_state['values']['node_type'];
  $class = $form_state['values']['class'];

  $result = db_query("SELECT DISTINCT(nid) FROM {content_type_{$node_type}} WHERE {$node_type}_class_value = '%s' AND {$node_type}_conid_value = %d", $class, $conid);

  $batch_title = t('Deleting all MLS Data for the %class class from the connection: @connection. ', array('%class' => $class, '@connection' => $connections[$conid]['name']));

  $nids = array();
  while ($nid = db_fetch_object($result)) {
    $nids[] = $nid->nid;
  }
  batch_set(array(
    'title' => $batch_title,
    'operations' => array(
      array('drealty_flush_batch', array($nids, $class)),
    ),
    'finished' => 'drealty_flush_batch_finished',
    'file' => drupal_get_path('module', 'drealty') . '/drealty.batch.inc',
  ));
}


/**
 * Confirm flushing of all listings. drupal_get_form() callback.
 */
function drealty_flush_all($form, $conid = NULL) {

  $connections = drealty_connections_fetch();
  $form = array();
  $form['conid'] = array('#type' => 'value', '#value' => $conid);

  $question =t('Are you sure you want to delete all the MLS Data for the %connection connection?', array('%connection' => $connections[$conid]['name']));
  $path = "admin/drealty/connection/manual_update/{$conid}";
  $description = t('The MLS data will be deleted from this server, but the data on the RETS server will not be modified and may be re-fetched if it still exists.
      This operation may take a significant amount of time dependent on the amout of data to delete; please be patient.');

  return confirm_form($form, $question, $path, $description, t('Delete'));
}

/**
 * Submission function for drealty_flush_all().
 *
 */
function drealty_flush_all_submit($form, &$form_state) {

  $connections = drealty_connections_fetch();
  $conid = $form_state['values']['conid'];

  $result = db_query("SELECT nid FROM {node} WHERE type = 'drealty_property' OR type = 'drealty_agent' OR type = 'drealty_office' OR type = 'drealty_open_house'", $conid);

  $batch_title = t('Deleting all MLS Data for connection: @connection. ', array('@connection' => $connections[$conid]['name']));

  $nids = array();
  while ($nid = db_fetch_object($result)) {
    $nids[] = $nid->nid;
  }

  batch_set(array(
    'title' => $batch_title,
    'operations' => array(
      array('drealty_flush_batch', array($nids, $class)),
    ),
    'finished' => 'drealty_flush_batch_finished',
    'file' => drupal_get_path('module', 'drealty') . '/drealty.batch.inc',
  ));
}
/**
 * Drupal Form to present the user with an interface to retrieve the data for an exported connection.
 *
 * @param array $form_state
 *  The Drupal $form_state array.
 * @param int $conid
 *  The connection id to export.
 * @return array
 */
function drealty_export_connection_form(&$form_state, $conid = NULL) {
  $form['export'] = array(
    '#title' => t('Export data'),
    '#type' => 'textarea',
    '#cols' => 60,
    '#value' => drealty_export_connection($conid),
    '#rows' => 40,
    '#description' => t('Copy the export text and paste it into another content type using the import function.'),
  );

  return $form;
}
/**
 * Function to construct the string that is presented for export.
 *
 * @param int $conid
 *  The connection id to export.
 * @return string
 *  A parsable string via php's var_export().
 *
 */
function drealty_export_connection($conid) {
  $string = "";

  $connections = drealty_connections_fetch();
  $connection = $connections[$conid];
  $resources = drealty_resources_fetch($conid);
  $resource_mappings = drealty_resource_mappings($conid);


  foreach ($resources as &$resource) {
    $resource['classes'] = drealty_classes_fetch($conid, $resource['systemName']);
    $resource['fields'] = drealty_fields_fetch($conid, $resource['systemName']);
    $resource['use_img'] = variable_get("drealty_use_img_{$resource['systemName']}_{$connection['conid']}", FALSE);
    $resource['use_loc'] = variable_get("drealty_use_loc_{$resource['systemName']}_{$connection['conid']}", FALSE);
  }
  $connection['mappings'] = $resource_mappings;
  $connection['resources'] = $resources;

  $connection['office_configured'] = variable_get('drealty_office_configured', FALSE);
  $connection['agent_configured'] = variable_get('drealty_agent_configured', FALSE);
  $connection['property_configured'] = variable_get('drealty_property_configured', FALSE);
  $connection['open_house_configured'] = variable_get('drealty_open_house_configured', FALSE);
  $string = serialize($resources);
  //$string .= "\$condata = " . var_export($connection, TRUE) . ";\n";
  //$string .= "\$condata['resources'] = " . var_export($resources, TRUE) . ";\n";

  return $string;
}
/**
 * Form to facilitate the importing of an exported connection.
 *
 * @param array $form_state
 * @return array
 *
 */
function drealty_import_connection_form(&$form_state) {
  // $form['#prefix'] = t('This form will import a connection definition exported from another instance of the dRealty IDX System.');
  $form['macro'] = array(
    '#type' => 'textarea',
    '#rows' => 40,
    '#title' => t('Import data'),
    '#required' => TRUE,
    '#description' => t('Paste the text created by a connection export into this field.'),
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Import'),
  );
  return $form;
}
/**
 * Implementation of hook_form_submit().
 *
 * @param array $form
 * @param array $form_state
 * @return NULL
 *
 */
function drealty_import_connection_form_submit($form, &$form_state) {
  if (user_access('import drealty configuration')) {
    $form_values = $form_state['values'];

    $condata = NULL;
    // Convert the import formatted text back into a $connection array.
    // Return if errors generated or not an array.
    // Use '@' to suppress errors about undefined constants in the macro.
    @eval($form_values['macro']);


    // Preliminary error trapping, must have valid arrays to work with.
    if (!isset($condata) || !isset($condata['login_url']) || !is_array($condata) || !is_array($condata['resources'])) {
      form_set_error('macro', t('The import data is not valid import text.'));
      return;
    }

    drealty_import_connection($condata);
  }
}
/**
 *
 * @param array $condata
 *
 */
function drealty_import_connection($condata) {

  $resource_count = 0;
  $class_count = 0;
  $field_count = 0;
  $cck_field_count = 0;
  $mapping_count = 0;

  // write the connection record
  $connection = array(
    'name' => $condata['name'],
    'login_url' => $condata['login_url'],
    'username' => $condata['username'],
    'password' => $condata['password'],
    'ua_string' => $condata['ua_string'],
    'version' => $condata['version'],
    'force_basic_auth' => $condata['force_basic_auth'],
    'use_compression' => $condata['use_compression'],
    'active' => $condata['active'],
  );

  drupal_write_record('drealty_connections', $connection, array());
  drupal_set_message(t("Successfully created connection - @connection.", array("@connection" => $condata['name'])));

  // write out the resources
  foreach ($condata['resources'] as $resource) {
    $res = array(
      'conid' => $connection['conid'],
      'systemName' => $resource['systemName'],
      'description' => $resource['description'],
      'lastupdate' => $resource['lastupdate'],
      'selection_values' => $resource['selection_values'],
      'keyfield' => $resource['keyfield'],
      'chunk_size' => $resource['chunk_size'],
    );
    drupal_write_record('drealty_resources', $res, array());
    watchdog("drealty", "Successfully created resource - @resource.", array("@resource" => $resource['systemName']));

    // write classes
    foreach ($resource['classes'] as $class) {
      // unset the cid
      unset($class['cid']);
      $class['conid'] = $connection['conid'];
      $class['rid'] = $resource['rid'];
      drupal_write_record('drealty_classes', $class, array());
      watchdog("drealty", "Successfully created a class for @resource named: @class.", array("@class" => $class['systemName'], "@resource" => $resource['systemName']));
      $class_count++;
    }

    // write fields
    foreach ($resource['fields'] as $field) {
      //unset the fid
      unset($field['fid']);
      $field['conid'] = $connection['conid'];
      $field['resource'] = $resource['systemName'];
      drupal_write_record('drealty_fields', $field, array());
      watchdog("drealty", "Successfully created a field for @resource named: @field.", array("@field" => $field['longName'], "@resource" => $resource['systemName']));
      $field_count++;
    }

    if ($resource['use_img']) {
      variable_set("drealty_use_img_{$resource['systemName']}_{$connection['conid']}", TRUE);
    }
    if ($resource['use_loc']) {
      variable_set("drealty_use_loc_{$resource['systemName']}_{$connection['conid']}", TRUE);
    }
    $resource_count++;
  }

  //write out the mappings
  foreach ($condata['mappings'] as $mapping) {
    unset($mapping['mid']);
    $mapping['conid'] = $connection['conid'];
    drupal_write_record('drealty_resource_mappings', $mapping, array());
    watchdog("drealty", "Successfully created mapping - @map_type => @map_node_type.", array("@map_type" => $mapping['resource'], "@map_node_type" => $mapping['node_type']));
    $mapping_count++;
  }

  // set any of the types as configured
  if ($connection['office_configured']) {
    variable_set('drealty_office_configured', TRUE);
  }
  if ($connection['agent_configured']) {
    variable_set('drealty_agent_configured', TRUE);
  }
  if ($connection['property_configured']) {
    variable_set('drealty_property_configured', TRUE);
  }
  if ($connection['open_house_configured']) {
    variable_set('drealty_open_house_configured', TRUE);
  }

  $conid = $connection['conid'];

  // populate lookups
  $resources = drealty_resources_fetch($condata['conid']);
  foreach ($resources as $resource) {
    drealty_populate_all_lookups($conid, $resource['systemName']);
  }


  // create the cck fields
  module_load_include('inc', 'content', 'includes/content.crud');

  $mappings = drealty_resource_mappings($conid);

  foreach ($mappings as $mapping) {

    // grab the fields we need to setup
    $active_fields = drealty_fields_active_fetch($conid, $mapping['resource']);

    // loop through the fields and set each one of them up
    foreach ($active_fields as $field) {

      if (!$field['mapped_field']) {
        $cck_field = array(
          'field_name' => 'dr_' . $field['cck_field_name'],
          'type_name' => $mapping['node_type'],
          'label' => $field['cck_label'],
          'group' => "{$mapping['node_type']}_group",
        );
        //type
        if ($field['interpretation'] === 'LookupMulti' || $field['interpretation'] === 'Lookup') {

          $values = array();
          $value_holder = array();
          $sort_by = array();
          $lookup_tables = unserialize($field['lookupName']);
          if (is_array($lookup_tables)) {
            foreach ($lookup_tables as $key => $value) {
              $lookups = drealty_lookups_fetch($conid, $mapping['resource'], $value, $key);
              $tmp_values = array();
              foreach ($lookups as $lup) {
                //$plain = check_plain($lup['longValue']);
                $tmp_values["{$lup['longValue']}"] = "{$lup['longValue']}|{$lup['longValue']}";
              }
              // array_merge() should remove any duplicates by merging them together since the key is a string.
              $value_holder = array_merge($tmp_values, $value_holder);
            }
            sort($value_holder);
            $values = array_values($value_holder);
          }
          else {
            watchdog("drealty", "Error creating lookup values for @cck_name", array("@cck_name" => $field['cck_field_name']), WATCHDOG_ERROR);
          }

          $cck_field['type'] = 'text';
          $cck_field['max_length'] = 60; //$field['max_length'];
          $cck_field['widget_type'] = 'optionwidgets_select';
          $cck_field['allowed_values'] = implode("\n", $values);
          $cck_field['multiple'] = $field['interpretation'] == 'LookupMulti' ? 1 : 0;
        }
        else {
          if (in_array($field['dataType'], array('Boolean', 'Tiny', 'Small', 'Int', 'Long')) || ($field['dataType'] === 'Decimal' && $field['field_precision'] == 0)) {
            $cck_field['type'] = 'number_integer';
            $cck_field['widget_type'] = 'number';
          }
          elseif ($field['dataType'] === 'Decimal') {
            $cck_field['type'] = 'number_decimal';
            $cck_field['widget_type'] = 'number';
            // Note that RETS and this CCK field are using the term "precision"
            // differently
            $cck_field['precision'] = $field['max_length'];
            $cck_field['scale'] = $field['field_precision'];
          }
          elseif (in_array($field['dataType'], array('Date', 'DateTime'))) {
            $cck_field['type'] = 'datestamp';
            $cck_field['widget_type'] = 'date_text';
            $cck_field['widget_settings'] = array(
              'input_format' => 'm/d/Y - H:i:s',
            ); // @todo: Format this according to Date module defaults?
            $cck_field['year_range'] = '-0:+1';
            $cck_field['granularity'] = array(
              'year' => 'year',
              'month' => 'month',
              'day' => 'day',
              'hour' => 'hour',
              'minute' => 'minute',
              'second' => 'second',
            );
          }
          else {
            // Assume all others are Character
            $cck_field['type'] = 'text';
            $cck_field['max_length'] = $field['max_length'];
            $cck_field['widget_type'] = $field['max_length'] > 127 ? 'text_textarea' : 'text_textfield';
          }
        }
        content_field_instance_create($cck_field, FALSE);
        if (module_exists('fieldgroup')) {
          fieldgroup_update_fields($cck_field);
        }
        if (module_exists('content_permissions') && module_exists('permissions_api')) {
          $permissions_view = array("view {$cck_field['field_name']}");
          $permissions_edit = array("edit {$cck_field['field_name']}");
          $roles = user_roles();
          foreach ($roles as $role) {
            permissions_grant_permissions($role, $permissions_view);
            permissions_revoke_permissions($role, $permissions_edit);
          }
        }
        watchdog("drealty", 'Created field %label (@id).', array('%label' => $cck_field['label'], '@id' => $field['systemName']));
        $cck_field_count++;
      }
    }

    //$conid, $mapping['resource']
    //check to see if we need location or image fields

    if (variable_get("drealty_use_img_{$mapping['resource']}_{$conid}", FALSE)) {
      $field_name = "dr_images";
      $image_field = content_field_instance_read(array('field_name' => $field_name));
      if (!$image_field) {
        $img = array(
          'field_name' => 'dr_images',
          'type_name' => $mapping['node_type'],
          'label' => t('Images'),
          'type' => 'filefield',
          'widget_type' => 'imagefield_widget',
          'file_extensions' => 'gif jpg jpeg png',
          'custom_alt' => FALSE,
          'custom_title' => FALSE,
          'group' => "{$resource}_group",
          'multiple' => TRUE,
          'weight' => -10,
        );

        content_field_instance_create($img, FALSE);
        if (module_exists('fieldgroup')) {
          fieldgroup_update_fields($img);
        }
        if (module_exists('content_permissions') && module_exists('permissions_api')) {
          $permissions_view = array("view {$img['field_name']}");
          $permissions_edit = array("edit {$img['field_name']}");
          $roles = user_roles();
          foreach ($roles as $role) {
            permissions_grant_permissions($role, $permissions_view);
            permissions_revoke_permissions($role, $permissions_edit);
          }
        }
      }
    }


    if (variable_get("drealty_use_loc_{$mapping['resource']}_{$conid}", FALSE)) {

      // check to see if we already have this field created
      $field_name = "{$mapping['node_type']}_loc";
      $location_field = content_field_instance_read(array('field_name' => $field_name));
      if (!$location_field) {
        // field doesn't exists so we'll create one
        $loc = drealty_get_loc_array($mapping['node_type']);
        module_load_include('inc', 'content', 'includes/content.crud');
        drupal_set_message(t('Creating <em>Location</em> field.'));
        content_field_instance_create($loc, FALSE);
        if (module_exists('fieldgroup')) {
          fieldgroup_update_fields($loc);
        }
        if (module_exists('content_permissions') && module_exists('permissions_api')) {
          $permissions_view = array("view {$loc['field_name']}");
          $permissions_edit = array("edit {$loc['field_name']}");
          $roles = user_roles();
          foreach ($roles as $role) {
            permissions_grant_permissions($role, $permissions_view);
            permissions_revoke_permissions($role, $permissions_edit);
          }
        }
      }
    }

  }



  drupal_set_message(t('The connection was successfully imported.<br /> Created the connection: @connection_name. With @resource_count resources, @class_count classes, @field_count fields, and @mapping_count mappings. Also created @cck_field_count cck_fields.', array('@connection_name' => $connection['name'], '@resource_count' => $resource_count, '@class_count' => $class_count, '@field_count' => $field_count, '@mapping_count' => $mapping_count, '@cck_field_count' => $cck_field_count)));
  content_clear_type_cache(TRUE);
  menu_rebuild();
  cache_clear_all("drealty_fields_", 'cache', TRUE);


}

function drealty_get_loc_array($type) {
  return array(
    'field_name' => "{$type}_loc",
    'type_name' => "$type",
    'display_settings' =>
    array(
      'label' =>
      array(
        'format' => 'above',
        'exclude' => 0,
      ),
      'teaser' =>
      array(
        'format' => 'default',
        'exclude' => 0,
      ),
      'full' =>
      array(
        'format' => 'default',
        'exclude' => 0,
      ),
      4 =>
      array(
        'format' => 'default',
        'exclude' => 0,
      ),
    ),
    'widget_active' => '1',
    'type' => 'location',
    'required' => '0',
    'multiple' => '0',
    'db_storage' => '1',
    'module' => 'location_cck',
    'active' => '1',
    'locked' => '0',
    'columns' =>
    array(
      'lid' =>
      array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => FALSE,
      ),
    ),
    'location_settings' =>
    array(
      'form' =>
      array(
        'fields' =>
        array(
          'name' =>
          array(
            'collect' => '0',
            'default' => '',
            'weight' => '2',
          ),
          'street' =>
          array(
            'collect' => '1',
            'default' => '',
            'weight' => '4',
          ),
          'additional' =>
          array(
            'collect' => '0',
            'default' => '',
            'weight' => '6',
          ),
          'city' =>
          array(
            'collect' => '0',
            'default' => '',
            'weight' => '8',
          ),
          'province' =>
          array(
            'collect' => '1',
            'default' => '',
            'weight' => '10',
          ),
          'postal_code' =>
          array(
            'collect' => '1',
            'default' => '',
            'weight' => '12',
          ),
          'country' =>
          array(
            'collect' => '4',
            'default' => 'us',
            'weight' => '14',
          ),
          'locpick' =>
          array(
            'collect' => '1',
            'weight' => '20',
          ),
        ),
      ),
      'display' =>
      array(
        'hide' =>
        array(
          'name' => 0,
          'street' => 0,
          'additional' => 0,
          'city' => 0,
          'province' => 0,
          'postal_code' => 0,
          'country' => 0,
          'locpick' => 0,
          'province_name' => 0,
          'country_name' => 0,
          'map_link' => 0,
          'coords' => 0,
        ),
      ),
    ),
    'widget' =>
    array(
      'default_value' =>
      array(
        0 =>
        array(
          'name' => '',
          'street' => '',
          'additional' => '',
          'country' => 'us',
          'locpick' =>
          array(
            'user_latitude' => '',
            'user_longitude' => '',
          ),
          'cck_preview_in_progress' => TRUE,
          'location_settings' =>
          array(
            'form' =>
            array(
              'fields' =>
              array(
                'name' =>
                array(
                  'default' => '',
                  'collect' => '1',
                  'weight' => '2',
                ),
                'street' =>
                array(
                  'default' => '',
                  'collect' => '1',
                  'weight' => '4',
                ),
                'additional' =>
                array(
                  'default' => '',
                  'collect' => '0',
                  'weight' => '6',
                ),
                'city' =>
                array(
                  'default' => '',
                  'collect' => '0',
                  'weight' => '8',
                ),
                'province' =>
                array(
                  'default' => '',
                  'collect' => '0',
                  'weight' => '10',
                ),
                'postal_code' =>
                array(
                  'default' => '',
                  'collect' => '0',
                  'weight' => '12',
                ),
                'country' =>
                array(
                  'default' => 'us',
                  'collect' => '1',
                  'weight' => '14',
                ),
                'locpick' =>
                array(
                  'default' =>
                  array(
                    'user_latitude' => '',
                    'user_longitude' => '',
                  ),
                  'collect' => '1',
                  'weight' => '20',
                  'nodiff' => TRUE,
                ),
                'cck_preview_in_progress' =>
                array(
                  'default' => TRUE,
                ),
                'location_settings' =>
                array(
                  'default' =>
                  array(
                    'form' =>
                    array(
                      'fields' =>
                      array(
                        'lid' =>
                        array(
                          'default' => FALSE,
                        ),
                        'name' =>
                        array(
                          'default' => '',
                          'collect' => 1,
                          'weight' => 2,
                        ),
                        'street' =>
                        array(
                          'default' => '',
                          'collect' => 1,
                          'weight' => 4,
                        ),
                        'additional' =>
                        array(
                          'default' => '',
                          'collect' => 1,
                          'weight' => 6,
                        ),
                        'city' =>
                        array(
                          'default' => '',
                          'collect' => 0,
                          'weight' => 8,
                        ),
                        'province' =>
                        array(
                          'default' => '',
                          'collect' => 0,
                          'weight' => 10,
                        ),
                        'postal_code' =>
                        array(
                          'default' => '',
                          'collect' => 0,
                          'weight' => 12,
                        ),
                        'country' =>
                        array(
                          'default' => 'us',
                          'collect' => 1,
                          'weight' => 14,
                        ),
                        'locpick' =>
                        array(
                          'default' => FALSE,
                          'collect' => 1,
                          'weight' => 20,
                          'nodiff' => TRUE,
                        ),
                        'latitude' =>
                        array(
                          'default' => 0,
                        ),
                        'longitude' =>
                        array(
                          'default' => 0,
                        ),
                        'source' =>
                        array(
                          'default' => 0,
                        ),
                        'is_primary' =>
                        array(
                          'default' => 0,
                        ),
                        'delete_location' =>
                        array(
                          'default' => FALSE,
                          'nodiff' => TRUE,
                        ),
                      ),
                    ),
                  ),
                ),
                'lid' =>
                array(
                  'default' => FALSE,
                ),
                'latitude' =>
                array(
                  'default' => 0,
                ),
                'longitude' =>
                array(
                  'default' => 0,
                ),
                'source' =>
                array(
                  'default' => 0,
                ),
                'is_primary' =>
                array(
                  'default' => 0,
                ),
                'delete_location' =>
                array(
                  'default' => FALSE,
                  'nodiff' => TRUE,
                ),
              ),
            ),
            'display' =>
            array(
              'hide' =>
              array(
                'name' => 0,
                'street' => 0,
                'additional' => 0,
                'city' => 0,
                'province' => 0,
                'postal_code' => 0,
                'country' => 0,
                'locpick' => 0,
                'province_name' => 0,
                'country_name' => 0,
                'map_link' => 0,
                'coords' => 0,
              ),
            ),
          ),
        ),
      ),
      'default_value_php' => NULL,
      'label' => t('Location'),
      'weight' => '51',
      'description' => '',
      'type' => 'location',
      'module' => 'location_cck',
    ),
  );
}