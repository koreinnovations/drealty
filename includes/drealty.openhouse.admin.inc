<?php

class DrealtyOpenHouseUIController extends EntityDefaultUIController {

  public function hook_menu() {

    $id_count = count(explode('/', $this->path));
    $wildcard = isset($this->entityInfo['admin ui']['menu wildcard']) ? $this->entityInfo['admin ui']['menu wildcard'] : '%' . $this->entityType;

    $items[$this->path] = array(
        'title' => 'dRealty Open Houses',
        'page callback' => 'drealty_openhouse_admin',
        'access arguments' => array('access drealty admin'),
        'type' => MENU_LOCAL_TASK | MENU_NORMAL_ITEM,
        'file' => 'drealty.openhouse.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
    );
    $items[$this->path . '/add'] = array(
        'title' => 'Add dRealty openhouse',
        'page callback' => 'drealty_openhouse_form_wrapper',
        'page arguments' => array(drealty_openhouse_create()),
        'access arguments' => array('create drealty openhouse'),
        'file' => 'drealty.openhouse.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
        'type' => MENU_LOCAL_ACTION,
    );
    // Loading and editing model entities
    $items[$this->path . '/' . $wildcard] = array(
        'title' => t('Edit openhouse'),
        'page callback' => 'drealty_openhouse_form_wrapper',
        'page arguments' => array($id_count + 1),
        'access callback' => 'drealty_access',
        'access arguments' => array('edit', $id_count + 1),
        'weight' => 0,
        'context' => MENU_CONTEXT_PAGE | MENU_CONTEXT_INLINE,
        'file' => 'drealty.openhouse.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes'
    );
    $items[$this->path . '/' . $wildcard . '/edit'] = array(
        'title' => 'Edit',
        'type' => MENU_DEFAULT_LOCAL_TASK,
        'weight' => -10,
        'context' => MENU_CONTEXT_PAGE | MENU_CONTEXT_INLINE,
    );

    $items[$this->path . '/' . $wildcard . '/delete'] = array(
        'title' => 'Delete',
        'page callback' => 'model_delete_form_wrapper',
        'page arguments' => array($id_count + 1),
        'access callback' => 'drealty_access',
        'access arguments' => array('delete', $id_count + 1),
        'type' => MENU_LOCAL_TASK,
        'context' => MENU_CONTEXT_INLINE,
        'weight' => 10,
        'file' => 'drealty.openhouse.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes'
    );


    $items['openhouses/' . $wildcard] = array(
        'title callback' => 'drealty_openhouse_page_title',
        'title arguments' => array(1),
        'page callback' => 'drealty_openhouse_page_view',
        'page arguments' => array(1),
        'access callback' => 'drealty_access',
        'access arguments' => array('view', 1),
        'type' => MENU_CALLBACK,
    );

    return $items;
  }

}

function drealty_openhouse_admin($type = 'new') {
  $edit = $_POST;
  if (isset($edit['operation']) && ($edit['operation'] == 'delete') && isset($edit['openhouses']) && $edit['openhouses']) {
    return drupal_get_form('drealty_openhouse_multiple_delete_confirm');
  } else {
    return drupal_get_form('drealty_openhouse_admin_overview', $type);
  }
}

function drealty_openhouse_admin_overview($form, &$form_state, $arg) {
  $header = array(
      'openhouse_id' => array('data' => t('openhouse Id'), 'field' => 'id'),
      'mls_id' => array('data' => t('MLS #'), 'field' => 'openhouse_id'),
      'date' => array('data' => t('Dates'), 'field' => 'start_datetime'),
      'address' => array('data' => t('Address')),
      'operations' => array('data' => t('Operations')),
  );

  $query = db_select('drealty_openhouse', 'd')->extend('PagerDefault')->extend('TableSort');
  $result = $query
    ->fields('d', array('id', 'openhouse_id'))
    ->limit(50)
    ->orderByHeader($header)
    ->execute();

  $lids = array();
  foreach ($result as $row) {
    $lids[] = $row->id;
  }

  $openhouses = entity_load('drealty_openhouse', $lids);

  $options = array();

  foreach ($openhouses as $openhouse) {
    $address = "{$openhouse->listing->street_number} {$openhouse->listing->street_dir_prefix} {$openhouse->listing->street_name} {$openhouse->listing->street_dir_suffix} {$openhouse->listing->street_suffix}, {$openhouse->listing->city}, {$openhouse->listing->state_or_province} {$openhouse->listing->postal_code}";
    $address = str_replace("  ", " ", $address);
    $links = menu_contextual_links('drealty', 'admin/content/drealty_openhouses', array($openhouse->listing_id));
    $options[$openhouse->id] = array(
        'openhouse_id' => $openhouse->id,
        'mls_id' => array('data' => array('#type' => 'link', '#title' => $openhouse->listing_id, '#href' => 'listings/' . $openhouse->listing_id)),
        'date' => date("l F dS, Y", $openhouse->start_datetime) . " From: " . date("g:ia", $openhouse->start_datetime) .' to ' . date("g:ia", $openhouse->end_datetime),
        'address' => $address,
        'operations' => theme('links', array('links' => $links, 'attributes' => array('class' => array('links', 'inline', 'operations')))),
    );
  }
  $form['openhouses'] = array(
      '#type' => 'tableselect',
      '#header' => $header,
      '#options' => $options,
      '#empty' => t('No openhouses available.'),
  );

  $form['pager'] = array('#theme' => 'pager');
  return $form;
}

function drealty_openhouse_form_wrapper($openhouse) {
  return drupal_get_form('drealty_openhouse_edit_form', $openhouse);
}

function drealty_openhouse_edit_form($form, &$form_state, $openhouse) {

  $form_state['openhouse'] = $openhouse;


  if ($openhouse->rets_imported) {
    $address = "{$openhouse->street_number} {$openhouse->street_dir_prefix} {$openhouse->street_name} {$openhouse->street_dir_suffix} {$openhouse->street_suffix}, {$openhouse->city}, {$openhouse->state_or_province} {$openhouse->postal_code}";
    $address = str_replace("  ", " ", $address);
    $form['address'] = array('#markup' => "<div><strong>Address: </strong>$address</div>");
    $form['mlsid'] = array('#markup' => "<div><strong>MSL#: </strong> {$openhouse->openhouse_id}</div>");
    $form['mlskey'] = array('#markup' => "<div><strong>MLS Key: </strong> {$openhouse->openhouse_key}</div>");
    $form['price'] = array('#markup' => "<div><strong>Price: </strong> {$openhouse->list_price}</div>");
  }



  field_attach_form('drealty_openhouse', $openhouse, $form, $form_state);

  $form['actions'] = array(
      '#type' => 'container',
      '#attributes' => array('class' => array('form-actions')),
      '#weight' => 400,
  );

  // We add the form's #submit array to this button along with the actual submit
  // handler to preserve any submit handlers added by a form callback_wrapper.
  $submit = array();

  if (!empty($form['#submit'])) {
    $submit += $form['#submit'];
  }

  $form['actions']['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save openhouse'),
      '#submit' => $submit + array('drealty_openhouse_edit_form_submit'),
  );

  if (!empty($openhouse->name)) {
    $form['actions']['delete'] = array(
        '#type' => 'submit',
        '#value' => t('Delete openhouse'),
        '#suffix' => l(t('Cancel'), 'admin/content/openhouses'),
        '#submit' => $submit + array('drealty_openhouse_form_submit_delete'),
        '#weight' => 45,
    );
  }

  // We append the validate handler to #validate in case a form callback_wrapper
  // is used to add validate handlers earlier.
  $form['#validate'][] = 'drealty_openhouse_edit_form_validate';
  return $form;
}