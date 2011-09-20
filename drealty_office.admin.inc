<?php

class DrealtyOfficeUIController extends EntityDefaultUIController {

  public function hook_menu() {

    $id_count = count(explode('/', $this->path));
    $wildcard = isset($this->entityInfo['admin ui']['menu wildcard']) ? $this->entityInfo['admin ui']['menu wildcard'] : '%' . $this->entityType;

    $items[$this->path] = array(
        'title' => 'dRealty Offices',
        'page callback' => 'drealty_office_admin',
        'access arguments' => array('access administration pages'),
        'type' => MENU_LOCAL_TASK | MENU_NORMAL_ITEM,
        'file' => 'drealty_office.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']),
    );
    $items[$this->path . '/add'] = array(
        'title' => 'Add dRealty Office',
        'page callback' => 'drealty_office_form_wrapper',
        'page arguments' => array(drealty_office_create()),
        'access callback' => 'drealty_office_access',
        'access arguments' => array('edit', $id_count + 1),
        'file' => 'drealty_office.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']),
        'type' => MENU_LOCAL_ACTION,
    );
    // Loading and editing model entities
    $items[$this->path . '/offices/' . $wildcard] = array(
        'title' => t('Edit office'),
        'page callback' => 'drealty_office_form_wrapper',
        'page arguments' => array($id_count + 1),
        'access callback' => 'drealty_office_access',
        'access arguments' => array('edit', $id_count + 1),
        'weight' => 0,
        'context' => MENU_CONTEXT_PAGE | MENU_CONTEXT_INLINE,
        'file' => 'drealty_office.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module'])
    );
    $items[$this->path . '/offices/' . $wildcard . '/edit'] = array(
        'title' => 'Edit',
        'type' => MENU_DEFAULT_LOCAL_TASK,
        'weight' => -10,
        'context' => MENU_CONTEXT_PAGE | MENU_CONTEXT_INLINE,
    );

    $items[$this->path . '/offices/' . $wildcard . '/delete'] = array(
        'title' => 'Delete',
        'page callback' => 'model_delete_form_wrapper',
        'page arguments' => array($id_count + 1),
        'access callback' => 'drealty_office_access',
        'access arguments' => array('edit', $id_count + 1),
        'type' => MENU_LOCAL_TASK,
        'context' => MENU_CONTEXT_INLINE,
        'weight' => 10,
        'file' => 'drealty_office.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module'])
    );


    $items['offices/' . $wildcard] = array(
        'title callback' => 'drealty_office_page_title',
        'title arguments' => array(1),
        'page callback' => 'drealty_office_page_view',
        'page arguments' => array(1),
        'access callback' => 'drealty_office_access',
        'access arguments' => array('view', 1),
        'type' => MENU_CALLBACK,
    );

    return $items;
  }

}

function drealty_office_admin($type = 'new') {
  $edit = $_POST;
  if (isset($edit['operation']) && ($edit['operation'] == 'delete') && isset($edit['offices']) && $edit['offices']) {
    return drupal_get_form('drealty_office_multiple_delete_confirm');
  } else {
    return drupal_get_form('drealty_office_admin_overview', $type);
  }
}

function drealty_office_admin_overview($form, &$form_state, $arg) {
  $header = array(
      'id' => array('data' => t('office Id'), 'field' => 'id'),
      'office_id' => array('data' => t('Office MLS Id'), 'field' => 'office_id'),
      'name' => array('data' => t('Name'), 'field' => 'name'),
      'phone' => array('data' => t('Phone')),
      'operations' => array('data' => t('Operations')),
  );

  $query = db_select('drealty_office', 'd')->extend('PagerDefault')->extend('TableSort');
  $result = $query
    ->fields('d', array('id', 'office_id'))
    ->limit(50)
    ->orderByHeader($header)
    ->execute();

  $lids = array();
  foreach ($result as $row) {
    $lids[] = $row->id;
  }

  $offices = entity_load('drealty_office', $lids);

  $options = array();

  foreach ($offices as $office) {
    $links = menu_contextual_links('model', 'admin/content/offices/office', array($office->office_id));
    $options[$office->id] = array(
        'id' => $office->id,
        'office_id' => $office->office_id,
        'name' => array('data' => array('#type' => 'link', '#title' => $office->name, '#href' => 'offices/' . $office->office_id)),
        'phone' => $office->phone,
        'operations' => theme('links', array('links' => $links, 'attributes' => array('class' => array('links', 'inline', 'operations')))),
    );
  }
  $form['offices'] = array(
      '#type' => 'tableselect',
      '#header' => $header,
      '#options' => $options,
      '#empty' => t('No offices available.'),
  );

  $form['pager'] = array('#theme' => 'pager');
  return $form;
}

function drealty_office_form_wrapper($office) {
  return drupal_get_form('drealty_office_edit_form', $office);
}

function drealty_office_edit_form($form, &$form_state, $office) {

  $form_state['office'] = $office;


  if ($office->rets_imported) {
    $form['name'] = array('#markup' => "<div><strong>Name: </strong>{$office->first_name} {$office->last_name}</div>");
    $form['mlsid'] = array('#markup' => "<div><strong>MSL Id: </strong> {$office->office_id}</div>");
    $form['phone'] = array('#markup' => "<div><strong>Office Phone: </strong> {$office->phone_office} {$office->phone_office_ext}</div>");
    $form['cell'] = array('#markup' => "<div><strong>Cell: </strong> {$office->phone_cell}</div>");
  }



  field_attach_form('drealty_office', $office, $form, $form_state);

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
      '#value' => t('Save office'),
      '#submit' => $submit + array('drealty_office_edit_form_submit'),
  );

  if (!empty($office->name)) {
    $form['actions']['delete'] = array(
        '#type' => 'submit',
        '#value' => t('Delete office'),
        '#suffix' => l(t('Cancel'), 'admin/content/offices'),
        '#submit' => $submit + array('drealty_office_form_submit_delete'),
        '#weight' => 45,
    );
  }

  // We append the validate handler to #validate in case a form callback_wrapper
  // is used to add validate handlers earlier.
  $form['#validate'][] = 'drealty_office_edit_form_validate';
  return $form;
}