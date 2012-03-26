<?php

class DrealtyLeadUIController extends EntityDefaultUIController {

  public function hook_menu() {

    $id_count = count(explode('/', $this->path));
    $wildcard = isset($this->entityInfo['admin ui']['menu wildcard']) ? $this->entityInfo['admin ui']['menu wildcard'] : '%' . $this->entityType;

    $items[$this->path] = array(
      'title' => 'dRealty Leads',
      'page callback' => 'drealty_lead_admin',
      'access arguments' => array('access drealty lead admin'),
      'type' => MENU_LOCAL_TASK | MENU_NORMAL_ITEM,
      'file' => 'drealty_lead.lead.admin.inc',
      'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
    );
		
    $items[$this->path . '/add'] = array(
      'title' => 'Add dRealty Lead',
      'page callback' => 'drealty_lead_form_wrapper',
      'access arguments' => array('create drealty lead'),
      'file' => 'drealty_lead.lead.admin.inc',
      'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
      'type' => MENU_LOCAL_ACTION,
    );
    
		// Loading and editing model entities
    $items[$this->path . '/' . $wildcard] = array(
      'title' => t('Edit Lead'),
      'page callback' => 'drealty_lead_form_wrapper',
      'page arguments' => array('edit', $id_count),
      'access callback' => 'drealty_lead_access',
      'access arguments' => array('edit', $id_count),
      'weight' => 0,
      'context' => MENU_CONTEXT_PAGE | MENU_CONTEXT_INLINE,
      'file' => 'drealty_lead.lead.admin.inc',
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
      'page callback' => 'drealty_lead_form_wrapper',
      'page arguments' => array('delete', $id_count),
      'access callback' => 'drealty_lead_access',
      'access arguments' => array('delete', $id_count),
      'type' => MENU_LOCAL_TASK,
      'context' => MENU_CONTEXT_INLINE,
      'weight' => 10,
      'file' => 'drealty.lead.admin.inc',
      'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes'
    );


    $items['drealty_lead/' . $wildcard] = array(
      'title callback' => 'drealty_lead_page_title',
      'title arguments' => array(1),
      'page callback' => 'drealty_lead_page_view',
      'page arguments' => array(1),
      'access callback' => 'drealty_lead_access',
      'access arguments' => array('view', 1),
      'type' => MENU_CALLBACK,
    );

    return $items;
  }

}

function drealty_lead_admin($type = 'new') {
  $edit = $_POST;
  if (isset($edit['operation']) && ($edit['operation'] == 'delete') && isset($edit['leads']) && $edit['leads']) {
    return drupal_get_form('drealty_lead_multiple_delete_confirm');
  } else {
    return drupal_get_form('drealty_lead_admin_overview', $type);
  }
}

function drealty_lead_admin_overview($form, &$form_state, $arg) {
  $header = array(
    'id' => array('data' => t('Lead ID'), 'field' => 'id'),
    'mls_id' => array('data' => t('MLS #'), 'field' => 'rets_id'),
    'operations' => array('data' => t('Operations')),
  );

  $query = db_select('drealty_lead', 'd')->extend('PagerDefault')->extend('TableSort');
  $result = $query
    ->fields('d', array('id', 'rets_id'))
    ->limit(10)
    ->orderByHeader($header)
    ->execute();

  $lids = array();
  foreach ($result as $row) {
    $lids[] = $row->id;
  }

  $leads = entity_load('drealty_lead', $lids);

  $options = array();

  foreach ($leads as $lead) {

    $links = menu_contextual_links('drealty', 'admin/content/drealty_leads', array($lead->id));

    $options[$listing->id] = array(
      'id' => $listing->id,
      'mls_id' => array('data' => array('#type' => 'link', '#title' => $lead->rets_id, '#href' => 'drealty_lead/' . $lead->id)),
      'operations' => theme('links', array('links' => $links, 'attributes' => array('class' => array('links', 'inline', 'operations')))),
    );
  }
  $form['listings'] = array(
    '#type' => 'tableselect',
    '#header' => $header,
    '#options' => $options,
    '#empty' => t('No listings available.'),
  );

  $form['pager'] = array('#theme' => 'pager');
  return $form;
}

function drealty_lead_form_wrapper($op, $listing) {
  return drupal_get_form('drealty_lead_edit_form', $op, $listing);
}

function drealty_lead_edit_form($form, &$form_state, $op, $listing) {

  $form_state['lead'] = $lead;

  field_attach_form('drealty_lead', $lead, $form, $form_state);

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
    '#value' => t('Save listing'),
    '#submit' => $submit + array('drealty_lead_edit_form_submit'),
  );

  if (!empty($listing->name) && !$listing->rets_imported) {
    $form['actions']['delete'] = array(
      '#type' => 'submit',
      '#value' => t('Delete lead'),
      '#suffix' => l(t('Cancel'), 'admin/content/drealty_leads'),
      '#submit' => $submit + array('drealty_lead_form_submit_delete'),
      '#weight' => 45,
    );
  }

  return $form;
}

function drealty_lead_edit_form_submit($form, &$form_state) {
  // grab the listing
  $lead = $form_state['lead'];
  // attach any fields from the field api
  field_attach_submit('drealty_lead', $listing, $form, $form_state);
  // save the listing
  $lead->save();
  // redirect back to the content list
  $form_state['redirect'] = 'admin/content/drealty_leads';
}
