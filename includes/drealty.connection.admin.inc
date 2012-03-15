<?php

class DrealtyConnectionUIController extends EntityDefaultUIController {

  /**
   * Overrides hook_menu() defaults.
   */
  public function hook_menu() {

    $items = parent::hook_menu();
    $id_count = count(explode('/', $this->path));
    $wildcard = isset($this->entityInfo['admin ui']['menu wildcard']) ? $this->entityInfo['admin ui']['menu wildcard'] : '%entity_object';

    $items[$this->path]['weight'] = 0;

    $items[$this->path . '/add']['title'] = t('add new drealty connection');
    $items[$this->path . '/import']['title'] = t('import drealty connection');

    $items[$this->path . '/manage/' . $wildcard . '/activate'] = array(
        'title' => 'Activate',
        'page callback' => 'drupal_get_form',
        'page arguments' => array('drealty_activate_connection_form', $id_count + 1),
        'access arguments' => array('access drealty admin'),
        'file' => 'drealty.connection.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
        'type' => MENU_CALLBACK,
    );

    $items[$this->path . '/manage/' . $wildcard . '/deactivate'] = array(
        'title' => 'Dectivate',
        'page callback' => 'drealty_deactivate_connection',
        'page arguments' => array($id_count + 1),
        'access arguments' => array('access drealty admin'),
        'file' => 'drealty.connection.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
        'type' => MENU_CALLBACK,
    );


    $items[$this->path . '/manage/' . $wildcard . '/delete'] = array(
        'title' => 'Delete',
        'page callback' => 'drealty_connection_delete_form_wrapper',
        'page arguments' => array($id_count + 1),
        'access arguments' => array('access drealty admin'),
        'type' => MENU_LOCAL_TASK,
        'context' => MENU_CONTEXT_INLINE,
        'weight' => 10,
        'file' => 'drealty.connection.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes'
    );

    $items[$this->path . '/manage/' . $wildcard . '/resource/%'] = array(
        'title callback' => 'drealty_configure_resource_title_callback',
        'title arguments' => array($id_count + 3),
        'page callback' => 'drupal_get_form',
        'page arguments' => array('drealty_configure_resource_form', $id_count + 1, $id_count + 3),
        'access arguments' => array('access drealty admin'),
        'file' => 'drealty.connection.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
        'type' => MENU_NORMAL_ITEM,
    );

    $items[$this->path . '/manage/' . $wildcard . '/resource/%/%drealty_class/fields'] = array(
        'title callback' => 'drealty_configure_fields_title_callback',
        'title arguments' => array($id_count + 4),
        'page callback' => 'drupal_get_form',
        'page arguments' => array('drealty_configure_resource_fields_form', $id_count + 1, $id_count + 3, $id_count + 4),
        'access arguments' => array('access drealty admin'),
        'file' => 'drealty.connection.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
        'type' => MENU_NORMAL_ITEM,
    );
    $items[$this->path . '/manage/' . $wildcard . '/resource/%/%drealty_class/limits'] = array(
        'title' => 'Configure Import Options',
        'page callback' => 'drupal_get_form',
        'page arguments' => array('drealty_configure_resource_limits_form', $id_count + 1, $id_count + 3, $id_count + 4),
        'access arguments' => array('access drealty admin'),
        'file' => 'drealty.connection.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
        'type' => MENU_NORMAL_ITEM,
    );
    return $items;
  }

  public function overviewTable($conditions = array()) {
    $entities = entity_load($this->entityType, FALSE, $conditions);
    ksort($entities);

    $rows = array();
    foreach ($entities as $entity) {

      $name = $entity->name;
      $active = $entity->active;


      $operation = '';
      if ($active) {
        $operation .= l("Deactivate", "admin/drealty/connections/manage/$name/deactivate");
        $operation .= ' <br /> ' . "Configure: " . l("Properties", "admin/drealty/connections/manage/$name/resource/drealty_listing");
        $operation .= ' | ' . l("Open Houses", "admin/drealty/connections/manage/$name/resource/drealty_openhouse");
        $operation .= ' | ' . l("Offices", "admin/drealty/connections/manage/$name/resource/drealty_office");
        $operation .= ' | ' . l("Agents", "admin/drealty/connections/manage/$name/resource/drealty_agent");
      } else {
        $operation .= l("Activate", "admin/drealty/connections/manage/$name/activate");
      }
      $additional = array($operation);

      $rows[] = $this->overviewTableRow($conditions, entity_id($this->entityType, $entity), $entity, $additional);
    }
    // Assemble the right table header.
    $header = array(t('Label'));
    // Add in the RETS Operations
    $header[] = t('dRealty Operations');
    if (!empty($this->entityInfo['exportable'])) {
      $header[] = t('Status');
    }
    // Add operations with the right colspan.
    $field_ui = !empty($this->entityInfo['bundle of']) && module_exists('field_ui');
    $exportable = !empty($this->entityInfo['exportable']);
    $colspan = 3;
    $colspan = $field_ui ? $colspan + 2 : $colspan;
    $colspan = $exportable ? $colspan + 1 : $colspan;
    $header[] = array('data' => t('Operations'), 'colspan' => $colspan);

    $render = array(
        '#theme' => 'table',
        '#header' => $header,
        '#rows' => $rows,
        '#empty' => t('None.'),
    );
    return $render;
  }

}

function drealty_configure_fields_title_callback($class) {
  return "Configure Fields for: {$class->standardname}";
}

function drealty_configure_resource_title_callback($type) {
  switch ($type) {
    case 'drealty_listing':
      $title = "dRealty Listing Resource Managment";
      break;
    case 'drealty_agent':
      $title = "dRealty Agent Resource Managment";
      break;
    case 'drealty_office':
      $title = "dRealty Office Resource Managment";
      break;
    case 'drealty_openhouse':
      $title = "dRealty Open House Resource Managment";
      break;
    default:
      $title = 'dRealty Field Management';
  }
  return $title;
}

function drealty_connection_entity_edit_form($form, &$form_state, $connection) {
  $form['conid'] = array('#type' => 'value', '#value' => $connection->conid);

  $form['label'] = array(
      '#type' => 'textfield',
      '#title' => t('Connection Label'),
      '#description' => t('The human-readable name of this connection.'),
      '#required' => TRUE,
      '#size' => 80,
      '#default_value' => isset($connection->label) ? $connection->label : '',
  );
  $form['name'] = array(
      '#type' => 'machine_name',
      '#title' => t('Connection Name'),
      '#machine_name' => array(
          'exists' => 'drealty_connections_get_all',
          'source' => array('label'),
      ),
      '#description' => t('A unique machine-readable name for this connection. It must only contain lowercase letters, numbers, and underscores.'),
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
  } else {

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
      $supported_caps = drupal_substr($capabilities, 0, drupal_strlen($capabilities) - 2);
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
    } else {
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

  $connection = new dRealtyConnectionEntity();
  $connection = $form_state['connection'];

  // this will also clear out the resources, classes, fieldmappings, and resourcemappings
  $connection->delete();

  drupal_set_message(t('The %name connection has been deleted.', array('%name' => $connection->name)));
  watchdog('drealty', 'Deleted %name connection.', array('%name' => $connection->name));

  $form_state['redirect'] = 'admin/drealty/connections';
}

function drealty_connection_delete_form_wrapper(dRealtyConnectionEntity $connection = NULL) {
  return drupal_get_form('drealty_connection_delete_form', $connection);
}

function drealty_select_resource_type($form, &$form_state) {
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
  } else {
    unset($mapping['mid']);
    drupal_write_record('drealty_resource_mappings', $mapping, array());
  }
  $conid = $connection->conid;
  cache_clear_all("drealty_mappings_$conid", 'cache');
  // set a succes message
  drupal_set_message(t('The resource type selection has been saved.'));
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
      '#type' => 'submit',
      '#value' => isset($mappings[$entity_type]) ? 'change' : 'select',
      '#submit' => array('drealty_select_resource_type'),
      '#suffix' => '<br /><br />',
  );

  if (isset($form_state['input']['prop_resource']) || isset($mappings[$entity_type])) {
    if (isset($form_state['input']['prop_resource'])) {
      $results = $connection->fetchClasses($form_state['input']['prop_resource']);
    } else {
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

    // setup the property types for the dropdown
    $bundle_types = entity_load('drealty_listing_type');
    $type_opts = array();
    $type_opts['none'] = '-- Select --';
    foreach ($bundle_types as $type) {
      $type_opts[$type->type] = $type->label;
    }
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
      $form['classes'][$key]['bundle'] = array('#type' => 'select', '#options' => $type_opts, '#default_value' => isset($class->bundle) ? $class->bundle : 'none',);
      $form['classes'][$key]['lifetime'] = array('#type' => 'select', '#options' => $life_opts, '#default_value' => isset($class->lifetime) ? $class->lifetime : 28000, '#disabled' => $class->configured ? FALSE : TRUE,);
      $configure_fields_link = l("configure id fields", "admin/drealty/connections/manage/{$connection->name}/resource/{$entity_type}/{$class->cid}/fields");
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
        '#submit' => array('drealty_configure_resource_form_submit'),
        '#value' => t('Save Changes'),
    );
  }
  return $form;
}

function drealty_configure_resource_form_validate($form, &$form_state) {
  foreach ($form_state['values']['classes'] as $class) {
    if ($class['enabled'] == TRUE && $class['bundle'] == 'none') {
      $name = $class['classname'];
      form_set_error("classes][$name][bundle", "Please select a property type.");
    }
  }
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


  $conid = $connection->conid;

  // clear out the cached data
  cache_clear_all("drealty_classes_", 'cache', TRUE);
  cache_clear_all("drealty_mappings_$conid", 'cache');
  // set a succes message
  drupal_set_message(t('The Configuration options have been Saved.'));
}

function drealty_configure_resource_fields_form($form, &$form_state, dRealtyConnectionEntity $connection = NULL, $entity_type = NULL, $class = NULL) {


  $form_state['connection'] = $connection;
  $form_state['entity_type'] = $entity_type;
  $form_state['class'] = $class;


  $mappings = $connection->resourceMappings();

  if (!isset($mappings[$entity_type])) {
    drupal_set_message(t('You must configure the @resource Resource type first.', array("@resource" => ucwords($entity_type))), 'error');
    return $form;
  }

  $field_mappings = $connection->FetchFieldMappings($mappings[$entity_type]->resource, $class->cid);
  $fields = $connection->FetchFields($mappings[$entity_type]->resource);

  $form['rets_id'] = array(
      '#type' => 'textfield',
      '#title' => t('Id Field'),
      '#description' => t('A unique ID field for the RETS resource, for Properties this is typically the MLS ID.'),
      '#required' => TRUE,
      '#default_value' => isset($field_mappings['rets_id']) ? $fields[$field_mappings['rets_id']->systemname]->systemname . ' - ' . $fields[$field_mappings['rets_id']->systemname]->longname : NULL,
      '#autocomplete_path' => "admin/drealty/fields_autocomplete/$connection->conid/$class->systemname",
  );

  $form['rets_key'] = array(
      '#type' => 'textfield',
      '#title' => t('Key Field'),
      '#description' => t('The unique key field for the RETS resource as defined by the RETS provider. Can be the same as the ID field, however some RETS providers have seperate fields for the Key and the ID. Images are also typically based off of the Key field rather than the ID field.'),
      '#required' => TRUE,
      '#default_value' => isset($field_mappings['rets_key']) ? $fields[$field_mappings['rets_key']->systemname]->systemname . ' - ' . $fields[$field_mappings['rets_key']->systemname]->longname : NULL,
      '#autocomplete_path' => "admin/drealty/fields_autocomplete/$connection->conid/$class->systemname",
  );


  $form['rets_status'] = array(
      '#type' => 'textfield',
      '#title' => t('Status Field'),
      '#description' => t('The RETS Listing status field. If your server supports offset, then this field must be mapped in order to build the default RETS query.'),
      '#required' => FALSE,
      '#default_value' => !empty($field_mappings['rets_status']) ? $fields[$field_mappings['rets_status']->systemname]->systemname . ' - ' . $fields[$field_mappings['rets_status']->systemname]->longname : NULL,
      '#autocomplete_path' => "admin/drealty/fields_autocomplete/$connection->conid/$class->systemname",
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
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

  $rets_id = explode(' - ', $form_state['values']['rets_id']);
  if (!isset($mls_fields[$rets_id[0]])) {
    form_set_error('[rets_id]', 'Mapping Error. The Id Field is configured to a value that does not exist.');
  }

  $rets_key = explode(' - ', $form_state['values']['rets_key']);
  if (!isset($mls_fields[$rets_key[0]])) {
    form_set_error('[rets_key]', 'Mapping Error. The Key Field is configured to a value that does not exist.');
  }
}

function drealty_configure_resource_fields_form_submit($form, &$form_state) {

  $entity_type = $form_state['entity_type'];
  $class = $form_state['class'];


  $connection = new dRealtyConnectionEntity();
  $connection = $form_state['connection'];

  $mappings = $connection->resourceMappings();
  $resource = $mappings[$entity_type]->resource;

  $rets_id = explode(' - ', $form_state['values']['rets_id']);
  $rets_key = explode(' - ', $form_state['values']['rets_key']);
  if (!empty($form_state['values']['rets_status'])) {
    $rets_status = explode(' - ', $form_state['values']['rets_status']);
    $connection->CreateFieldMapMapping($resource, 'rets_status', $rets_status[0], $class->cid);
  }
  $connection->CreateFieldMapMapping($resource, 'rets_id', $rets_id[0], $class->cid);
  $connection->CreateFieldMapMapping($resource, 'rets_key', $rets_key[0], $class->cid);

  cache_clear_all("drealty_fields_", 'cache', TRUE);
  cache_clear_all('drealty_field_mapping_', 'cache', TRUE);

  // since we have added some fields we need to clear the hash value on the affected listings so that when 
  // an import is run again we get the new values inserted

  db_update($entity_type)
          ->fields(array(
              'hash' => '0',
          ))
          ->condition('conid', $connection->conid)
          ->condition('class', $class->cid)
          ->execute();


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


  $form_state['connection'] = $connection;
  $form_state['entity_type'] = $entity_type;
  $form_state['resource'] = $resource;
  $form_state['class'] = $class;

  $fieldmappings = $connection->FetchFieldMappings($resource, $class->cid);
  

  
  
  $fields = $connection->FetchFields($resource);


  switch ($entity_type) {
    case 'drealty_listing':
    case 'drealty_openhouse':
      $query_field = 'rets_status';
      break;
    case 'drealty_agent':
    case 'drealty_office':
      $query_field = 'type';
      break;
    default:
      $query_field = FALSE;
  }

  $skip_status = FALSE;
  $query_types = array(0 => 'Default (Server must support offset. Relies on RETS Status Field.)', 1 => 'Offset <strong>Not</strong> Supported (Query based on price field)', 3 => 'Offset <strong>Not</strong> Supported (Query based on RETS Key)', 2 => 'Manually supplied Query (Server must support offset)');
  
  if ($query_field === FALSE || !isset($fieldmappings[$query_field])) {
    $skip_status = TRUE;
    unset($query_types[0]);
  }

  $opts = array();
  $opts_object_types = array();
  $has_images = FALSE;

  if (!$skip_status) {
    $tables = unserialize($fields[$fieldmappings[$query_field]->systemname]->lookupname);
    $lookup_table = $tables[$class->systemname];

    if (empty($lookup_table)) {
      drupal_set_message(t('There was an error in your selection of the status field. Please ensure you have selected the correct MLS field. The <strong>@fieldname</strong> is not a valid type for the status.', array("@fieldname" => $fieldmappings['listing_status']->systemname)), 'error');
      drupal_goto("admin/drealty/connections/manage/{$connection->name}/resource/$entity_type");
    }
  }
  $rets = new dRealtyConnection();


  if ($rets->connect($connection->conid)) {
    if (!$skip_status) {
      $values = $rets->get_phrets()->GetLookupValues($resource, $lookup_table);
      foreach ($values as $val) {
        $opts[$val['Value']] = check_plain($val['LongValue']);
      }
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

  $form['query_options'] = array('#type' => 'fieldset', '#title' => 'Query Options', '#collapsed' => FALSE);





  $form['query_options']['query_type'] = array(
      '#type' => 'radios',
      '#title' => t('Query Type'),
      '#default_value' => isset($class->query_type) ? $class->query_type : 0,
      '#options' => $query_types,
      '#description' => t('Please select the type of query operation to use.'),
  );

  if (!$skip_status) {
    $form['query_options']['status'] = array(
        '#type' => 'select',
        '#title' => t('Status Field'),
        '#options' => $opts,
        '#default_value' => explode(',', $class->status_values),
        '#description' => t('Select the appropriate values to limit the search query.'),
        '#multiple' => TRUE,
        '#states' => array(
            'visible' => array(':input[name="query_type"]' => array('value' => '0')),
            'required' => array(':input[name="query_type"]' => array('value' => '0')),
        ),
    );
  } else {
    $form_state['status_null'] = TRUE;
//    $form['query_options']['no_status'] = array(
//        '#markup' => '<div>You must configure the status field</div>',
//        '#states' => array(
//            'visible' => array(':input[name="query_type"]' => array('value' => '0')),
//            'required' => array(':input[name="query_type"]' => array('value' => '0')),
//        ),
//    );
  }
  $form['query_options']['status_override_query'] = array(
      '#type' => 'textfield',
      '#title' => t('Query to use'),
      '#size' => 100,
      '#description' => t('Must be a valid DMQL query. (specs found <a href="http://retsdoc.onconfluence.com/display/rets172/7.7+Query+language" target="_blank">here.</a> A good tutorial can be found <a href="http://www.flexmls.com/support/rets/tutorials/dmql/" target="_blank">here.</a> You may also find that <a href="http://www.retsmd.com" target="_blank">www.retsmd.com</a> is a valuable resource in helping write the query.'),
      '#default_value' => $class->override_status_query_text,
      '#states' => array(
          'visible' => array(':input[name="query_type"]' => array('value' => '2')),
          'required' => array(':input[name="query_type"]' => array('value' => '2')),
      ),
  );
  $form['query_options']['total_query'] = array(
      '#type' => 'textfield',
      '#title' => t('Query to use'),
      '#size' => 100,
      '#description' => t('Must be a valid DMQL query. (specs found <a href="http://retsdoc.onconfluence.com/display/rets172/7.7+Query+language" target="_blank">here.</a> A good tutorial can be found <a href="http://www.flexmls.com/support/rets/tutorials/dmql/" target="_blank">here.</a> You may also find that <a href="http://www.retsmd.com" target="_blank">www.retsmd.com</a> is a valuable resource in helping write the query.'),
      '#default_value' => $class->override_status_query_text,
      '#states' => array(
          'visible' => array(':input[name="query_type"]' => array('value' => '1')),
          'required' => array(':input[name="query_type"]' => array('value' => '1')),
      ),
  );
  $form['query_options']['check_query_1'] = array(
      '#type' => 'submit',
      '#value' => t('Test DMQL Query'),
      '#description' => t('This process my take several minutes.'),
      '#submit' => array('drealty_test_manual_query'),
      '#states' => array(
          'visible' => array(':input[name="query_type"]' => array('value' => '1')),
      ),
  );
  $form['query_options']['check_query'] = array(
      '#type' => 'submit',
      '#value' => t('Test DMQL Query'),
      '#description' => t('This process my take several minutes.'),
      '#submit' => array('drealty_test_manual_query'),
      '#states' => array(
          'visible' => array(':input[name="query_type"]' => array('value' => '2')),
      ),
  );

  $form['query_options']['offset_field'] = array(
      '#type' => 'textfield',
      '#title' => t('Price field to base the offset on'),
      '#description' => t('This field must be a numeric price field. If you are unsure, visit <a href="http://www.retsmd.com" target="_blank">www.retsmd.com</a> to ensure that the field is numeric.'),
      '#default_value' => $class->offset_field,
      '#autocomplete_path' => "admin/drealty/fields_autocomplete/$connection->conid/$class->systemname",
      '#states' => array(
          'visible' => array(':input[name="query_type"]' => array('value' => '1')),
          'required' => array(':input[name="query_type"]' => array('value' => '1')),
      ),
  );

  $form['query_options']['offset_amount'] = array(
      '#type' => 'select',
      '#title' => t('The amount to offset by.'),
      '#default_value' => $class->offset_amount,
      '#options' => array(10000 => '$10,000', 25000 => '$25,000', 50000 => '$50,000', 75000 => '$75,000', 100000 => '$100,000', 200000 => '$200,000'),
      '#states' => array(
          'visible' => array(':input[name="query_type"]' => array('value' => '1')),
          'required' => array(':input[name="query_type"]' => array('value' => '1')),
      ),
  );

  $form['query_options']['offset_max'] = array(
      '#type' => 'select',
      '#title' => t('The maximum offset amount.'),
      '#default_value' => $class->offset_max,
      '#options' => array(1000000 => '$1m', 1500000 => '$1.5m', 2000000 => '$2m', 5000000 => '$5m', 7500000 => '$7.5m', 10000000 => '$10m', 999000000 => '$999m'),
      '#states' => array(
          'visible' => array(':input[name="query_type"]' => array('value' => '1')),
          'required' => array(':input[name="query_type"]' => array('value' => '1')),
      ),
  );




  $form['query_options']['chunk'] = array(
      '#title' => t('Record chunk size'),
      '#type' => 'select',
      '#options' => drupal_map_assoc(array(250, 500, 1000, 1500, 2000, 3000)) + array('0' => t('Unlimited')),
      '#default_value' => $class->chunk_size,
      '#description' => t('When processing incoming records, the system will work with data structures containing this many records simultaneously,
      and cache the rest for later. If updating listings is often failing due to out of memory errors and increasing PHP&rsquo;s memory allocation is not an option,
      try decreasing this value. However, decreasing this value increases the amount of time it will take to update listings. The RETS server may override this value (MAXROWS).'),
      '#states' => array(
          'invisible' => array(':input[name="query_type"]' => array('value' => '1')),
      ),
  );





  if ($has_images) {
    $form['images'] = array('#type' => 'fieldset', '#title' => 'Images', '#collapsed' => FALSE);

    $img_field_opts = array();
    $field_instances = field_read_instances(array('bundle' => $class->bundle));

    foreach ($field_instances as $field_instance) {
      $field = field_read_field($field_instance['field_name']);
      if ($field['type'] == 'image') {
        $img_field_opts[$field['field_name']] = $field_instance['label'];
      }
    }

    if (count($img_field_opts) <= 0) {
      $form['images']['no_image_field'] = array(
          '#markup' => t('<div class="messages warning">There are no image fields defined for the Property Type associated with this class. In order to associate images you will need to create an image field to hold them.</div>'),
          '#states' => array(
              'visible' => array(':input[name="process_images"]' => array('checked' => TRUE)),
          ),
      );
    }

    $form['images']['process_images'] = array(
        '#type' => 'checkbox',
        '#title' => t('Process Images?'),
        '#description' => t('Process images/metadata objects for this resource type?'),
        '#default_value' => $class->process_images,
        '#disabled' => (count($img_field_opts) >= 1) ? FALSE : TRUE,
    );

    $form['images']['object_type'] = array(
        '#type' => 'select',
        '#multiple' => FALSE,
        '#title' => t('Object types to download'),
        '#description' => t('Each MLS provides zero or more object types (photos) to download for each listing. Select the object type you would like to download.'),
        '#options' => $opts_object_types,
        '#default_value' => $class->object_type,
        '#states' => array(
            'visible' => array(':input[name="process_images"]' => array('checked' => TRUE)),
        ),
    );



    if (count($img_field_opts) >= 1) {
      $form['images']['image_field_name'] = array(
          '#type' => 'select',
          '#multiple' => FALSE,
          '#title' => t('Image Field'),
          '#description' => t('Select the Image Field you would like to store the images in. You will need to create an image field if there are none to select.'),
          '#options' => $img_field_opts,
          '#default_value' => NULL,
          '#states' => array(
              'visible' => array(':input[name="process_images"]' => array('checked' => TRUE)),
          ),
      );
    }

    $chunk_opts = array('5' => '5', '10' => '10', '25' => '25', '40' => '40', '50' => '50');
    $form['images']['image_chunk_size'] = array(
        '#type' => 'select',
        '#multiple' => FALSE,
        '#title' => t('Image Chunk Size'),
        '#description' => t('Images are downloaded in batches, a listing can have 0 or more images associated with it. The chunk size determines how many listings are included in a batch. If each listing has 10 images associated with it and the chunk size
          is set to 10 a total of 100 images would potentially be downloaded at one time. This setting is meant to <em>adjust</em> the number of images that are downloaded in a single round trip to the RETS server. The more images downloaded the more 
          memory that is used by the system to process them. <strong>If you are experiencing out of memory errors while the system is processing images try decreasing the size.</strong> Default is 25'),
        '#options' => $chunk_opts,
        '#default_value' => isset($class->image_chunk_size) ? $class->image_chunk_size : '25',
        '#states' => array(
            'visible' => array(':input[name="process_images"]' => array('checked' => TRUE)),
        ),
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
  $entity_type = $form_state['entity_type'];
  $class = $form_state['class'];

  // grab the mappings and see if we have one already
  $mappings = $connection->ResourceMappings();

  $res = $mappings[$entity_type]->resource;

  $resources = $connection->Resources();

  $record = $resources[$res];
  $record->configured = TRUE;
  drupal_write_record('drealty_resources', $record, array('rid'));

  if (isset($form_state['values']['status'])) {
    $class->status_values = implode(',', $form_state['values']['status']);
  }
  $class->chunk_size = $form_state['values']['chunk'];
  $class->query_type = $form_state['values']['query_type'];
  switch ($class->query_type) {
    case 1:
      $class->override_status_query_text = $form_state['values']['total_query'];
      break;
    default:
      $class->override_status_query_text = $form_state['values']['status_override_query'];
  }
  //split out the system name of the field
  $tmp = explode(' - ', $form_state['values']['offset_field']);
  $systemname = $tmp[0];

  $class->offset_field = $systemname;
  $class->offset_amount = (int) $form_state['values']['offset_amount'];
  $class->offset_max = (int) $form_state['values']['offset_max'];


  if ($form_state['has_images'] && $form_state['values']['process_images']) {
    $class->process_images = $form_state['values']['process_images'];
    $class->object_type = $form_state['values']['object_type'];
    $class->image_field_name = $form_state['values']['image_field_name'];
    $field = field_read_instance($entity_type, $class->image_field_name, $class->bundle);
    $class->image_dir = isset($field['settings']['file_directory']) ? $field['settings']['file_directory'] : "rets_images";
  } else {
    $class->process_images = FALSE;
    $class->object_type = NULL;
  }
  $class->configured = TRUE;
  drupal_write_record('drealty_classes', $class, 'cid');

  cache_clear_all('drealty_classes_', 'cache', TRUE);
  cache_clear_all("drealty_resources_$connection->conid", 'cache');
  drupal_set_message(t("The configuration options have been saved."));
}

function drealty_test_manual_query($form, &$form_state) {

  drupal_set_message("Testing the DMQL Query.");

  $connection = new dRealtyConnectionEntity();
  $connection = $form_state['connection'];
  $resource = $form_state['resource'];
  $query = $form_state['values']['status_override_query'];
  $class = $form_state['class'];

  $dc = new dRealtyConnection();

  if ($dc->connect($connection->conid)) {

    $options = array(
        'count' => 1,
        'Format' => 'COMPACT-DECODED',
    );

    $search = $dc->get_phrets()->SearchQuery($resource, $class->systemname, $query);

    if ($dc->get_phrets()->Error()) {
      $error = $dc->get_phrets()->Error();
      drupal_set_message("Rets Error: {$error['code']}: {$error['text']}.", 'error');
    } else {
      $num_rows_returned = $dc->get_phrets()->NumRows($search);
      $total_records = $dc->get_phrets()->TotalRecordsFound($search);
      drupal_set_message("Success. RETS server returned $num_rows_returned of $total_records results.");
    }

    $dc->get_phrets()->FreeResult($search);

    $dc->disconnect();
  }

//  $class->override_status_query_text = $query;
//  drupal_write_record('drealty_classes', $class, 'cid');
//  cache_clear_all('drealty_classes_', 'cache', TRUE);
}

function theme_drealty_classes_form(&$variables) {
  $header = array('System Name', 'Description', 'Enabled', 'Property Type', 'Lifetime', 'Actions');
  $rows = array();
  $form = $variables['form'];
  foreach (element_children($form) as $key) {
    $row = array();
    $row[] = array('data' => drupal_render($form[$key]['name']));
    $row[] = array('data' => drupal_render($form[$key]['description']));
    $row[] = array('data' => drupal_render($form[$key]['enabled']));
    $row[] = array('data' => drupal_render($form[$key]['bundle']));
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
