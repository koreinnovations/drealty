<?php

class DrealtyConnectionUIController extends EntityDefaultUIController {

  /**
   * Overrides hook_menu() defaults.
   */
  public function hook_menu() {

    $items = parent::hook_menu();
    $id_count = count(explode('/', $this->path));
    $wildcard = isset($this->entityInfo['admin ui']['menu wildcard']) ? $this->entityInfo['admin ui']['menu wildcard'] : '%entity_object';

    $items[$this->path . '/add']['title'] = t('add new drealty connection');
    $items[$this->path . '/import']['title'] = t('import drealty connection');

    $items[$this->path . '/manage/' . $wildcard . '/activate'] = array(
        'title' => 'Activate',
        'page callback' => 'drupal_get_form',
        'page arguments' => array('drealty_activate_connection_form', $id_count + 1),
        'access arguments' => array('access administration pages'),
        'file' => 'drealty_connection.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']),
        'type' => MENU_CALLBACK,
    );

    $items[$this->path . '/manage/' . $wildcard . '/deactivate'] = array(
        'title' => 'Dectivate',
        'page callback' => 'drealty_deactivate_connection',
        'page arguments' => array($id_count + 1),
        'access arguments' => array('access administration pages'),
        'file' => 'drealty_connection.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']),
        'type' => MENU_CALLBACK,
    );

    $items[$this->path . '/manage/' . $wildcard . '/tools'] = array(
        'title' => 'Developer Tools',
        'page callback' => 'drupal_get_form',
        'page arguments' => array('drealty_developer_tools_form', $id_count + 1),
        'access arguments' => array('access administration pages'),
        'file' => 'drealty_connection.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']),
        'type' => MENU_CALLBACK,
    );


    $items[$this->path . '/manage/' . $wildcard . '/delete'] = array(
        'title' => 'Delete',
        'page callback' => 'drealty_connection_delete_form_wrapper',
        'page arguments' => array($id_count + 1),
        'access arguments' => array('access administration pages'),
        'type' => MENU_LOCAL_TASK,
        'context' => MENU_CONTEXT_INLINE,
        'weight' => 10,
        'file' => 'drealty_connection.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module'])
    );

    $items[$this->path . '/manage/' . $wildcard . '/resource/%'] = array(
        'title' => 'Configure Resources',
        'page callback' => 'drupal_get_form',
        'page arguments' => array('drealty_configure_resource_form', $id_count + 1, $id_count + 3),
        'access arguments' => array('access administration pages'),
        'file' => 'drealty_connection.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']),
        'type' => MENU_NORMAL_ITEM,
    );

    $items[$this->path . '/manage/' . $wildcard . '/resource/%/%drealty_class/fields'] = array(
        'title' => 'Configure Fields',
        'page callback' => 'drupal_get_form',
        'page arguments' => array('drealty_configure_resource_fields_form', $id_count + 1, $id_count + 3, $id_count + 4),
        'access arguments' => array('access administration pages'),
        'file' => 'drealty_connection.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']),
        'type' => MENU_NORMAL_ITEM,
    );
    $items[$this->path . '/manage/' . $wildcard . '/resource/%/%drealty_class/limits'] = array(
        'title' => 'Configure Limits',
        'page callback' => 'drupal_get_form',
        'page arguments' => array('drealty_configure_resource_limits_form', $id_count + 1, $id_count + 3, $id_count + 4),
        'access arguments' => array('access administration pages'),
        'file' => 'drealty_connection.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']),
        'type' => MENU_NORMAL_ITEM,
    );


    return $items;
  }

}

function drealty_connection_entity_edit_form($form, &$form_state, $connection) {
  $form['conid'] = array('#type' => 'value', '#value' => $connection->conid);

  $form['name'] = array(
      '#type' => 'textfield',
      '#title' => t('Connection Name'),
      '#description' => t('Name to identify the connection in the administration pages.'),
      '#required' => TRUE,
      '#size' => 80,
      '#default_value' => isset($connection->name) ? $connection->name : '',
  );
  $form['login_url'] = array(
      '#type' => 'textfield',
      '#title' => t('Login URL'),
      '#default_value' => isset($connection->login_url) ? $connection->login_url : '',
      '#description' => t('Login URL given to you by your RETS provider. <em>i.e. (http://demo.crt.realtors.org:6103/rets/login)</em>'),
      '#required' => TRUE,
      '#size' => 80,
  );
  $form['username'] = array(
      '#type' => 'textfield',
      '#title' => t('Username'),
      '#default_value' => isset($connection->username) ? $connection->username : '',
      '#size' => 40,
      '#required' => TRUE,
  );
  $form['password'] = array(
      '#type' => 'textfield',
      '#title' => t('Password'),
      '#default_value' => isset($connection->password) ? $connection->password : '',
      '#size' => 40,
      '#required' => TRUE,
  );

  $form['ua_string'] = array(
      '#type' => 'textfield',
      '#title' => t('User Agent String'),
      '#description' => t('A User Agent String.'),
      '#required' => TRUE,
      '#size' => 80,
      '#default_value' => isset($connection->ua_string) ? $connection->ua_string : 'dRealty/1.0',
  );
  $form['ua_password'] = array(
      '#type' => 'textfield',
      '#title' => t('User Agent Password'),
      '#description' => t('(Leave blank if you don\'t have one).'),
      '#required' => FALSE,
      '#size' => 40,
      '#default_value' => isset($connection->ua_password) ? $connection->ua_password : 'dRealty/1.0',
  );
  $rets_versions = array('1.0' => '1.0', '1.5' => '1.5', '1.7' => '1.7', '1.7.2' => '1.7.2');
  $form['version'] = array(
      '#type' => 'select',
      '#title' => t('RETS Version'),
      '#options' => $rets_versions,
      '#required' => TRUE,
      '#default_value' => isset($connection->version) ? $connection->version : '1.5',
  );
  $form['use_interealty_auth'] = array(
      '#type' => 'radios',
      '#title' => t('Use Interealty Authentication'),
      '#options' => array(TRUE => 'yes', FALSE => 'no'),
      '#required' => TRUE,
      '#default_value' => isset($connection->use_interealty_auth) ? $connection->use_interealty_auth : '0',
  );
  $form['force_basic_auth'] = array(
      '#type' => 'radios',
      '#title' => t('Force Basic Authentication'),
      '#options' => array(TRUE => 'yes', FALSE => 'no'),
      '#required' => TRUE,
      '#default_value' => isset($connection->force_basic_auth) ? $connection->force_basic_auth : '0',
  );
  $form['use_compression'] = array(
      '#type' => 'radios',
      '#title' => t('Use Compression'),
      '#options' => array(TRUE => 'yes', FALSE => 'no'),
      '#required' => TRUE,
      '#default_value' => isset($connection->use_compression) ? $connection->use_compression : '0',
  );
  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save Connection'),
      '#weight' => 40,
  );
  return $form;
}

/**
 * Generates the model type editing form.
 */
function drealty_connection_entity_form($form, &$form_state, $connection, $op = 'view') {
  switch ($op) {
    case 'edit':
    case 'add':
      return drealty_connection_entity_edit_form($form, $form_state, $connection);
    case 'view':
      return drupal_get_form('drealty_configure_resource_form', $connection->conid, 'property');
  }
}

/**
 * Form API submit callback for the type form.
 */
function drealty_connection_entity_form_submit(&$form, &$form_state) {
  $drealty_connection = entity_ui_form_submit_build_entity($form, $form_state);
  $drealty_connection->save();
  $form_state['redirect'] = 'admin/drealty/connections';
}

function drealty_activate_connection_form($form, &$form_state, $connection = NULL) {

  $form_state['connection'] = $connection;
  if (!isset($connection->conid)) {
    drupal_set_message(t("Must have a connection selected."), 'error');
  }
  else {

    $rets = new drealtyConnection();

    if ($rets->connect($connection->conid)) {

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

      $form['activate'] = array(
          '#type' => 'submit',
          '#value' => t('Activate'),
      );
    }
    else {
      drupal_set_message($rets->error_string, 'error');
      $form['markup'] = array(
          '#markup' => '<div>There was an error activating the current connection. Please check the settings and attempt to activate the connection again.</div>'
      );
    }


    $rets->disconnect();
  }
  return $form;
}

function drealty_activate_connection_form_submit($form, &$form_state) {

  $connection = $form_state['connection'];
  $connection->active = TRUE;
  $connection->save();
  drupal_set_message(t('The @name connection has been activated.', array('@name' => $connection->name)));
  $form_state['redirect'] = 'admin/drealty/connections';
}

function drealty_deactivate_connection(dRealtyConnectionEntity $connection = NULL) {
  $connection->active = FALSE;
  $connection->save();
  drupal_goto('admin/drealty/connections');
}

function drealty_connection_delete_form($form, &$form_state, dRealtyConnectionEntity $connection) {
  $form_state['connection'] = $connection;

  $form['#submit'][] = 'drealty_connection_delete_form_submit';

  $form = confirm_form($form, t('Are you sure you want to delete the %name connection?', array('%name' => $connection->name)), 'admin/drealty/connections', '<p>' . t('This action cannot be undone.') . '</p>', t('Delete'), t('Cancel'), 'confirm'
  );

  return $form;
}

function drealty_connection_delete_form_submit($form, &$form_state) {
  $connection = $form_state['connection'];

  $connection->delete();

  drupal_set_message(t('The %name connection has been deleted.', array('%name' => $connection->name)));
  watchdog('drealty', 'Deleted %name connection.', array('%name' => $connection->name));

  $form_state['redirect'] = 'admin/drealty/connections';
}

function drealty_connection_delete_form_wrapper(dRealtyConnectionEntity $connection = NULL) {
  return drupal_get_form('drealty_connection_delete_form', $connection);
}

function drealty_configure_resource_form($form, &$form_state, dRealtyConnectionEntity $connection = NULL, $entity_type = NULL) {


  if (!isset($connection)) {
    drupal_set_message(t("Must have a connection slected."), 'error');
  }

  $form_state['connection'] = $connection;
  $form_state['entity_type'] = $entity_type;

// grab the mappings and see if we have one already
  $mappings = $connection->resourceMappings();

  $results = $connection->resources();

  $resources = array();
  foreach ($results as $res) {
    $resources[$res->systemname] = $res->systemname;
  }

  $form['mid'] = array('#type' => 'value', '#value' => isset($mappings[$entity_type]) ? $mappings[$entity_type]->mid : NULL);
  $form['conid'] = array('#type' => 'value', '#value' => $connection->conid);
  $form['resource'] = array('#type' => 'value', '#value' => $entity_type);
  $form['prop_resource'] = array(
      '#type' => 'select',
      '#title' => t('@resource Resource', array('@resource' => ucwords(str_replace("_", " ", $entity_type)))),
      '#default_value' => isset($mappings[$entity_type]) ? $mappings[$entity_type]->resource : NULL,
      '#options' => $resources,
  );
  $form['select_prop_resource'] = array(
      '#type' => 'button',
      '#value' => t('Select'),
      '#submit' => array('drealty_connection_configure'),
      '#suffix' => '<br /><br />',
  );

  if (isset($form_state['input']['prop_resource']) || isset($mappings[$entity_type])) {
    if (isset($form_state['input']['prop_resource'])) {
      $results = $connection->fetchClasses($form_state['input']['prop_resource']);
    }
    else {
      $results = $connection->fetchClasses($mappings[$entity_type]->resource);
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

    $form['help'] = array('#markup' => "
      <h3>Instructions</h3>
      <div style='margin-bottom: 1.5em;'>To enable a class, configure the fields and limits, once this has been done the enable option will become available.</div>
    ");

    $form['classes']['#theme'] = 'drealty_classes_form';
    $form['classes']['#tree'] = TRUE;
    foreach ($results as $key => $class) {

      $form['classes'][$key]['name'] = array('#markup' => $class->systemname);
      $form['classes'][$key]['description'] = array('#markup' => "{$class->standardname} {$class->description}");

      $form['classes'][$key]['cname'] = array('#value' => $class->systemname);
      $form['classes'][$key]['sname'] = array('#value' => "{$class->standardname} {$class->description}");

      $form['classes'][$key]['enabled'] = array('#type' => 'checkbox', '#default_value' => $class->enabled, '#attributes' => $class->enabled ? array('checked' => 'checked') : NULL, '#disabled' => $class->configured ? FALSE : TRUE,);

      $form['classes'][$key]['lifetime'] = array('#type' => 'select', '#options' => $life_opts, '#default_value' => isset($class->lifetime) ? $class->lifetime : 28000, '#disabled' => $class->configured ? FALSE : TRUE,);
      $configure_fields_link = l("configure fields", "admin/drealty/connections/manage/{$connection->name}/resource/{$entity_type}/{$class->cid}/fields");
      $configure_limits_link = l("configure options", "admin/drealty/connections/manage/{$connection->name}/resource/{$entity_type}/{$class->cid}/limits");
      $form['classes'][$key]['actions'] = array('#markup' => "$configure_fields_link | $configure_limits_link");
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

function drealty_configure_resource_form_submit($form, &$form_state) {

  $connection = $form_state['connection'];

  $entity_type = $form_state['entity_type'];

// update the resource mapping for this connection
  $mapping = array(
      'mid' => isset($form_state['values']['mid']) ? $form_state['values']['mid'] : NULL,
      'conid' => $connection->conid,
      'resource' => $form_state['values']['prop_resource'],
      'entity_type' => $entity_type,
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
    $select_opts[] = "{$class['classname']}|{$class['standardname']}";
  }


  $conid = $connection->conid;

  // clear out the cached data
  cache_clear_all("drealty_classes_", 'cache', TRUE);
  cache_clear_all("drealty_mappings_$conid", 'cache');
  // set a succes message
  drupal_set_message(t('The Configuration options have been Saved.'));
}

function drealty_configure_resource_fields_form($form, &$form_state, dRealtyConnectionEntity $connection = NULL, $entity_type = NULL, $class = NULL) {

  if (empty($entity_type)) {
    drupal_set_message("Error");
  }

  drupal_add_css("div.description {
    white-space: normal;
    }", array('type' => 'inline'));

  $form_state['connection'] = $connection;
  $form_state['entity_type'] = $entity_type;
  $form_state['class'] = $class;

  //$form['fields']['#tree'] = TRUE;


  $listing_info = entity_get_property_info($entity_type);
  $props = $listing_info['properties'];

  $pattern = '/(.*?[a-z]{1})([A-Z]{1}.*?)/';
  $replace = '${1} ${2}';

  $properties = array();
  foreach ($props as &$prop) {
    $prop['label'] = preg_replace($pattern, $replace, $prop['label']);
  }

  $mappings = $connection->resourceMappings();

  if (!isset($mappings[$entity_type])) {
    drupal_set_message(t('You must configure the @resource Resource type first.', array("@resource" => ucwords($entity_type))), 'error');
    return $form;
  }

  $field_mappings = $connection->FetchFieldMappings($mappings[$entity_type]->resource, $class->cid);
  $fields = $connection->FetchFields($mappings[$entity_type]->resource);

  switch ($entity_type) {
    case 'drealty_listing':

      $form['sections']['required'] = array('#type' => 'fieldset', '#title' => 'Required', '#collapsible' => TRUE, '#collapsed' => FALSE);
      $form['sections']['general'] = array('#type' => 'fieldset', '#title' => 'General', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['Listing:Location:Address'] = array('#type' => 'fieldset', '#title' => 'Address', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['Listing:Location:School'] = array('#type' => 'fieldset', '#title' => 'School', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['Listing:Location:Area'] = array('#type' => 'fieldset', '#title' => 'Area', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['Listing:Location:GIS'] = array('#type' => 'fieldset', '#title' => 'GIS / Mapping', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['Listing:AgentOffice:ListAgent'] = array('#type' => 'fieldset', '#title' => 'Listing Agent', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['Listing:AgentOffice:ListOffice'] = array('#type' => 'fieldset', '#title' => 'Listing Office', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['Listing:AgentOffice:CoListAgent'] = array('#type' => 'fieldset', '#title' => 'Co-Listing Agent', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['Listing:AgentOffice:CoListOffice'] = array('#type' => 'fieldset', '#title' => 'Co-Listing Office', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['Listing:Property'] = array('#type' => 'fieldset', '#title' => 'Property Information', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['Listing:Property:Structure'] = array('#type' => 'fieldset', '#title' => 'Structure', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['Listing:Property:Rooms'] = array('#type' => 'fieldset', '#title' => 'Individual Rooms', '#collapsible' => TRUE, '#collapsed' => TRUE);

      $form['sections']['required']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['general']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['Listing:Location:Address']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['Listing:Location:School']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['Listing:Location:Area']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['Listing:Location:GIS']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['Listing:AgentOffice:ListAgent']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['Listing:AgentOffice:ListOffice']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['Listing:AgentOffice:CoListAgent']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['Listing:AgentOffice:CoListOffice']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['Listing:Property']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['Listing:Property:Structure']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['Listing:Property:Rooms']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');

      $form['sections']['Listing:Location:GIS']['settings'] = array('#type' => 'fieldset', '#title' => 'Settings', '#collapsible' => TRUE, '#collapsed' => FALSE, '#weight' => -1, '#description' => 'Special settings for the dRealty IDX system concerning GIS/Mapping');
      $form['sections']['Listing:Location:GIS']['settings']['geocode'] = array(
          '#type' => 'checkbox',
          '#title' => t('Perform Geocoding'),
          '#description' => t('Not all MLS systems provide geocoding for listings. If you would like dRealty to geocode the listing for you make sure to enable this setting.'),
          '#default_value' => $class->do_geocoding,
      );
      break;
    case 'drealty_openhouse':
      $form['sections']['required'] = array('#type' => 'fieldset', '#title' => 'Required', '#collapsible' => TRUE, '#collapsed' => FALSE);
      $form['sections']['general'] = array('#type' => 'fieldset', '#title' => 'General', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['date'] = array('#type' => 'fieldset', '#title' => 'Date(s)', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['OpenHouse:Location:Address'] = array('#type' => 'fieldset', '#title' => 'Address', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['OpenHouse:Location:Area'] = array('#type' => 'fieldset', '#title' => 'Area', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['OpenHouse:Location:GIS'] = array('#type' => 'fieldset', '#title' => 'GIS / Mapping', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['OpenHouse:AgentOffice:ListOffice'] = array('#type' => 'fieldset', '#title' => 'Listing Office', '#collapsible' => TRUE, '#collapsed' => TRUE);

      $form['sections']['required']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['general']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['date']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['OpenHouse:Location:Address']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['OpenHouse:Location:Area']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['OpenHouse:Location:GIS']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['OpenHouse:AgentOffice:ListOffice']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      break;

    case 'drealty_agent':
      $form['sections']['required'] = array('#type' => 'fieldset', '#title' => 'Required', '#collapsible' => TRUE, '#collapsed' => FALSE);
      $form['sections']['general'] = array('#type' => 'fieldset', '#title' => 'General', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['contact'] = array('#type' => 'fieldset', '#title' => 'Contact', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['address'] = array('#type' => 'fieldset', '#title' => 'Address', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['office'] = array('#type' => 'fieldset', '#title' => 'Office', '#collapsible' => TRUE, '#collapsed' => TRUE);

      $form['sections']['required']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['general']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['contact']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['address']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['office']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      break;

    case 'drealty_office':
      $form['sections']['required'] = array('#type' => 'fieldset', '#title' => 'Required', '#collapsible' => TRUE, '#collapsed' => FALSE);
      $form['sections']['general'] = array('#type' => 'fieldset', '#title' => 'General', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['contact'] = array('#type' => 'fieldset', '#title' => 'Contact', '#collapsible' => TRUE, '#collapsed' => TRUE);
      $form['sections']['address'] = array('#type' => 'fieldset', '#title' => 'Address', '#collapsible' => TRUE, '#collapsed' => TRUE);

      $form['sections']['required']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['general']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['contact']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      $form['sections']['address']['fields'] = array('#tree' => TRUE, '#theme' => 'drealty_fields_form');
      break;
  }

  asort($props);
  foreach ($props as $key => $property) {
    if (isset($property['group']) && $property['group'] != 'hidden') {
      $form['sections'][$property['group']]['fields'][$key]['display'] = array(
          '#type' => 'checkbox',
          '#title' => t('@sysname', array('@sysname' => $property['label'])),
          '#default_value' => isset($field_mappings[$key]) ? TRUE : FALSE,
          '#description' => $property['description'],
          '#required' => isset($property['required']) ? $property['required'] : FALSE,
          '#weight' => isset($property['weight']) ? $property['weight'] : 99,
      );
      $form['sections'][$property['group']]['fields'][$key]['field_name'] = array(
          '#type' => 'textfield',
          '#default_value' => isset($field_mappings[$key]) ? $fields[$field_mappings[$key]->systemname]->systemname . ' - ' . $fields[$field_mappings[$key]->systemname]->longname : NULL,
          '#autocomplete_path' => "admin/drealty/fields_autocomplete/$connection->conid/$class->systemname",
          '#required' => isset($property['required']) ? $property['required'] : FALSE,
          '#weight' => isset($property['weight']) ? $property['weight'] : 99,
      );
    }
  }


  $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save Configuration'),
  );
  return $form;
}

function drealty_configure_resource_fields_form_validate($form, &$form_state) {
  $connection = $form_state['connection'];
  $entity_type = $form_state['entity_type'];
  $class = $form_state['class'];

  $mappings = $connection->resourceMappings();

  $field_mappings = $connection->FetchFieldMappings($mappings[$entity_type]->resource, $class->cid);
  $mls_fields = $connection->FetchFields($mappings[$entity_type]->resource);


  $fields = $form_state['values']['fields'];

  foreach ($fields as $key => $field) {

    $tmp = explode(' - ', $field['field_name']);
    $systemname = $tmp[0];

    if (!isset($mls_fields[$systemname]) && $field['display']) {
      form_set_error("fields][$group][$key][field_name", "Mapping Error. The field named <strong>$systemname</strong> was not found.");
    }
  }
}

function drealty_configure_resource_fields_form_submit($form, &$form_state) {

  $entity_type = $form_state['entity_type'];
  $class = $form_state['class'];


  $connection = new dRealtyConnectionEntity();
  $connection = $form_state['connection'];

  $mappings = $connection->resourceMappings();
  $resource = $mappings[$entity_type]->resource;

  $fields = $form_state['values']['fields'];

  foreach ($fields as $key => $field) {
    $tmp = explode(' - ', $field['field_name']);
    $systemname = $tmp[0];

    if ($field['display']) {
      $connection->CreateFieldMapMapping($resource, $key, $systemname, $class->cid);
    }
    else {
      $connection->RemoveFieldMapping($resource, $key, $class->cid);
    }
  }


  $class->do_geocoding = $form_state['values']['geocode'];
  drupal_write_record('drealty_classes', $class, array('cid'));

  cache_clear_all("drealty_fields_", 'cache', TRUE);
  cache_clear_all('drealty_field_mapping_', 'cache', TRUE);
  drupal_set_message('Configuration options have been saved.');
}

function drealty_configure_resource_limits_form($form, &$form_state, dRealtyConnectionEntity $connection = NULL, $entity_type = NULL, $class = NULL) {


  // grab the mappings and see if we have one already
  $mappings = $connection->ResourceMappings();
  if (!isset($mappings[$entity_type])) {
    drupal_set_message(t('You must configure the @resource Resource type first.', array("@resource" => ucwords($entity_type))), 'error');
    drupal_goto("admin/drealty/connections/manage/{$connection->name}/resource/$entity_type");
  }

  $resource = $mappings[$entity_type]->resource;
  $resources = $connection->Resources();

  $form_state['connection'] = $connection;
  $form_state['resource'] = $entity_type;
  $form_state['class'] = $class;

  $fieldmappings = $connection->FetchFieldMappings($resource, $class->cid);
  $fields = $connection->FetchFields($resource);


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
      drupal_set_message(t('Please configure the fields before you attempt to configure the limits.', array('@status' => $status)), 'error');
      drupal_goto("admin/drealty/connections/manage/{$connection->name}/resource/$entity_type");
  }

  if (!isset($fieldmappings[$query_field])) {
    drupal_set_message(t('Please configure the fields before you attempt to configure the limits.', array('@status' => $status)), 'warning');
    drupal_goto("admin/drealty/connections/manage/{$connection->name}/resource/$entity_type");
  }

  $is_active = '';
  $opts = array();
  $opts_object_types = array();
  $has_images = FALSE;

  $tables = unserialize($fields[$fieldmappings[$query_field]->systemname]->lookupname);
  $lookup_table = $tables[$class->systemname];

  if (empty($lookup_table)) {
    drupal_set_message(t('There was an error in your selection of the status field. Please ensure you have selected the correct MLS field. The <strong>@fieldname</strong> is not a valid type for the status.', array("@fieldname" => $fieldmappings['listing_status']->systemname)), 'error');
    drupal_goto("admin/drealty/connections/manage/{$connection->name}/resource/$entity_type");
  }

  $rets = new dRealtyConnection();


  if ($rets->connect($connection->conid)) {
    $values = $rets->get_phrets()->GetLookupValues($resource, $lookup_table);
    foreach ($values as $val) {
      $opts[$val['Value']] = check_plain($val['LongValue']);
    }

    $object_types = $rets->get_phrets()->GetMetadataObjects($resource);
    if ($object_types) {
      $has_images = TRUE;
      foreach ($object_types as $type) {
        $opts_object_types[$type['ObjectType']] = $type['ObjectType'] . ' - ' . $type['Description'];
      }
    }
  }

  $form_state['has_images'] = $has_images;

  $form['status'] = array(
      '#type' => 'select',
      '#title' => t('Status Field'),
      '#required' => TRUE,
      '#options' => $opts,
      '#default_value' => explode(',', $class->status_values),
      '#description' => t('Select the appropriate values to limit the search query.'),
      '#multiple' => TRUE,
  );

  $form['status_override'] = array(
      '#type' => 'checkbox',
      '#title' => t('Override with the following query.'),
      '#default_value' => $class->override_status_query,
  );

  $form['status_override_query'] = array(
      '#type' => 'textfield',
      '#title' => t('Query to use'),
      '#default_value' => $class->override_status_query_text,
  );

  $form['chunk'] = array(
      '#title' => t('Record chunk size'),
      '#type' => 'select',
      '#options' => drupal_map_assoc(array(250, 500, 1000, 1500, 2000, 3000)) + array('0' => t('Unlimited')),
      '#default_value' => $class->chunk_size,
      '#description' => t('When processing incoming records, the system will work with data structures containing this many records simultaneously,
      and cache the rest for later. If updating listings is often failing due to out of memory errors and increasing PHP&rsquo;s memory allocation is not an option,
      try decreasing this value. However, decreasing this value increases the amount of time it will take to update listings. The RETS server may override this value (MAXROWS).'),
  );

  $form['format'] = array(
      '#title' => t('Record Format'),
      '#type' => 'select',
      '#options' => array(
          'COMPACT' => 'COMPACT',
          'COMPACT-DECODED' => 'COMPACT-DECODED',
      ),
      '#default_value' => ($class->format) ? $class->format : 'COMPACT-DECODED',
  );

  if ($has_images) {
    $form['images'] = array('#type' => 'fieldset', '#title' => 'Images', '#collapsed' => FALSE);
    $form['images']['process_images'] = array(
        '#type' => 'checkbox',
        '#title' => t('Process Images?'),
        '#description' => t('Process images/metadata objects for this resource type?'),
        '#default_value' => $class->process_images,
    );
    $form['images']['download_images'] = array(
        '#type' => 'checkbox',
        '#title' => t('Download Images?'),
        '#description' => t('If checked, images will be downloaded locally. Otherwise, images will be linked externally'),
        '#default_value' => $class->download_images,
    );

    $form['images']['object_type'] = array(
        '#type' => 'select',
        '#multiple' => FALSE,
        '#title' => t('Object types to download'),
        '#description' => t('Each MLS provides zero or more object types (photos) to download for each listing. Select the object type you would like to download.'),
        '#options' => $opts_object_types,
        '#default_value' => $class->object_type,
    );
  }
  $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save Configuration'),
  );

  return $form;
}

function drealty_configure_resource_limits_form_submit($form, &$form_state) {
  $connection = new dRealtyConnectionEntity();
  $connection = $form_state['connection'];
  $entity_type = $form_state['resource'];
  $class = $form_state['class'];

  // grab the mappings and see if we have one already
  $mappings = $connection->ResourceMappings();

  $res = $mappings[$entity_type]->resource;

  $resources = $connection->Resources();

  $record = $resources[$res];
  $record->configured = TRUE;
  drupal_write_record('drealty_resources', $record, array('rid'));

  $class->format = $form_state['values']['format'];
  $class->status_values = implode(',', $form_state['values']['status']);
  $class->chunk_size = $form_state['values']['chunk'];
  $class->override_status_query = $form_state['values']['status_override'];
  $class->override_status_query_text = $form_state['values']['status_override_query'];

  if ($form_state['has_images']) {
    $class->process_images = $form_state['values']['process_images'];
    $class->download_images = $form_state['values']['download_images'];
    $class->object_type = $form_state['values']['object_type'];
  }
  else {
    $class->process_images = FALSE;
    $class->download_images = FALSE;
    $class->object_type = NULL;
  }
  $class->configured = TRUE;
  drupal_write_record('drealty_classes', $class, 'cid');

  cache_clear_all('drealty_classes_', 'cache', TRUE);
  cache_clear_all("drealty_resources_$connection->conid", 'cache');
  drupal_set_message(t("The configuration options have been saved."));
  // set a variable that can be check to see if a particular 'type' has been configured.
}

function theme_drealty_classes_form(&$variables) {
  $header = array('System Name', 'Description', 'Enabled', 'Lifetime', 'Actions');
  $rows = array();
  $form = $variables['form'];
  foreach (element_children($form) as $key) {
    $row = array();
    $row[] = array('data' => drupal_render($form[$key]['name']));
    $row[] = array('data' => drupal_render($form[$key]['description']));
    $row[] = array('data' => drupal_render($form[$key]['enabled']));
    $row[] = array('data' => drupal_render($form[$key]['lifetime']));
    $row[] = array('data' => drupal_render($form[$key]['actions']));
    $rows[] = $row;
  }
  $output = theme('table', array('header' => $header, 'rows' => $rows));
  //$output .= drupal_render($form);
  return $output;
}

function theme_drealty_fields_form(&$variables) {
  $header = array('System Field', 'MLS Field Mapping');
  $rows = array();
  $form = $variables['form'];

  foreach (element_children($form) as $key) {
    $row = array();
    $row[] = array('data' => drupal_render($form[$key]['display']), 'style' => 'width: 60%;');
    $row[] = array('data' => drupal_render($form[$key]['field_name']), 'style' => 'width: 40%;');
    $rows[] = $row;
  }
  $output = theme('table', array('header' => $header, 'rows' => $rows));

  return $output;
}

function drealty_developer_tools_form_ajax_resource($form, $form_state) {
  return $form['cid'];
}

function drealty_developer_tools_form_ajax_class($form, $form_state) {
  return $form['fields_table_container'];
}

/**
 * Page callback for deactivating connections.
 */
function drealty_developer_tools_form($form, &$form_state, drealtyConnectionEntity $connection = NULL) {
  $form_state['connection'] = $connection;
  $form_ready = FALSE;


  if ($form_state['storage']['results']) {
    $form['results'] = array(
        '#type' => 'textarea',
        '#title' => t('Query Results'),
        '#default_value' => print_r($form_state['storage']['results'], TRUE),
    );
  }


  $dr = new drealtyResources();
  //$mappings = $connection->ResourceMappings();
  $resources = $connection->Resources();


  $resource_options = array(
      '' => t('Select a resource'),
  );
  $class_options = array();

  if ($form_state['values']['resource']) {
    $classes = $connection->FetchClasses($form_state['values']['resource']);
    $class_options[''] = t('Select a class');

    if ($form_state['values']['cid']) {
      $class = $dr->FetchClass($form_state['values']['cid']);
      $fields = $connection->FetchFields($form_state['values']['resource']);
      $form_ready = TRUE;
    }
  }

  foreach ($resources as $r) {
    $resource_options[$r->systemname] = $r->systemname;
  }

  foreach ($classes as $c) {
    $class_options[$c->cid] = $c->visiblename;
  }

  $form['resource'] = array(
      '#type' => 'select',
      '#title' => t('Resource'),
      '#required' => TRUE,
      '#options' => $resource_options,
      '#ajax' => array(
          'callback' => 'drealty_developer_tools_form_ajax_resource',
          'wrapper' => 'edit-class-wrapper',
      ),
  );

  $form['cid'] = array(
      '#type' => 'select',
      '#title' => t('Class'),
      '#required' => TRUE,
      '#options' => $class_options,
      '#prefix' => '<div id="edit-class-wrapper">',
      '#suffix' => '</div>',
      '#ajax' => array(
          'callback' => 'drealty_developer_tools_form_ajax_class',
          'wrapper' => 'fields-table-container-wrapper',
      ),
      '#disabled' => empty($form_state['values']['resource']),
  );

  $form['query'] = array(
      '#type' => 'textarea',
      '#title' => t('RETS Query'),
      '#required' => TRUE,
  );

  $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Run Query'),
  );

  $form['fields_table_container'] = array(
      '#type' => 'fieldset',
      '#title' => t('Field Mappings'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#prefix' => '<div id="fields-table-container-wrapper">',
      '#suffix' => '</div>',
  );

  if ($fields) {
    $form['fields_table_container']['fields_table'] = array(
        '#markup' => theme('drealty_fields_table', array('fields' => $fields)),
    );
    $form['fields_table_container']['#collapsed'] = FALSE;
  }

  return $form;
}

function drealty_developer_tools_form_submit($form, &$form_state) {
  $daemon = new drealtyDaemon();
  $dr = new drealtyResources();

  $class = $dr->FetchClass($form_state['values']['cid']);

  $results = $daemon->perform_query($form_state['connection'], $form_state['values']['resource'], $class, $form_state['values']['query']);

  $form_state['rebuild'] = TRUE;
  $form_state['storage']['results'] = $results;
}