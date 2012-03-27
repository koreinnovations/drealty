<?php

class DrealtyLeadUIController extends EntityDefaultUIController {

  public function hook_menu() {

    $id_count = count(explode('/', $this->path));
    $wildcard = isset($this->entityInfo['admin ui']['menu wildcard']) ? $this->entityInfo['admin ui']['menu wildcard'] : '%' . $this->entityType;
		
		$items[$this->path] = array(
      'title' => 'Drealty Leads',
      'description' => t('Manage Drealty Leads'),
      'page callback' => 'drealty_lead_admin',
      'access arguments' => array('access drealty lead admin'),
      'type' => MENU_LOCAL_TASK | MENU_NORMAL_ITEM,
      'file' => 'drealty_lead.lead.admin.inc',
      'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
    );
		
    $items['drealty_lead/add'] = array(
      'title' => 'Add dRealty Lead',
      'page callback' => 'drealty_lead_add_page',
      'access arguments' => array('create drealty lead'),
      'file' => 'drealty_lead.lead.admin.inc',
      'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
      'type' => MENU_NORMAL_ITEM,
    );
		
    $items['drealty_lead/' . $wildcard] = array(
      'title callback' => 'drealty_lead_page_title',
      'title arguments' => array(1),
      'page callback' => 'drealty_lead_view',
      'page arguments' => array(1),
      'access callback' => 'drealty_lead_access',
      'access arguments' => array('view', 1),
      'type' => MENU_CALLBACK,
			'file' => 'drealty_lead.lead.admin.inc',
      'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes'
    );

    $items['drealty_lead/' . $wildcard . '/view'] = array(
      'title' => 'View',
      'type' => MENU_DEFAULT_LOCAL_TASK,
      'weight' => -10,
			'file' => 'drealty_lead.lead.admin.inc',
      'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes'
    );

    // Loading and editing model entities
    $items['drealty_lead/' . $wildcard . '/edit'] = array(
      'title' => t('Edit Lead'),
      'page callback' => 'drealty_lead_form_wrapper',
      'page arguments' => array('edit', 1),
      'access callback' => 'drealty_lead_access',
      'access arguments' => array('edit', 1),
      'weight' => 0,
      'context' => MENU_CONTEXT_PAGE | MENU_CONTEXT_INLINE,
      'type' => MENU_LOCAL_TASK,
      'file' => 'drealty_lead.lead.admin.inc',
      'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes'
    );

    $items['drealty_lead/' . $wildcard . '/delete'] = array(
      'title' => 'Delete',
      'page callback' => 'drealty_lead_form_wrapper',
      'page arguments' => array('delete', 1),
      'access callback' => 'drealty_lead_access',
      'access arguments' => array('delete', 1),
      'type' => MENU_LOCAL_TASK,
      'context' => MENU_CONTEXT_INLINE,
      'weight' => 10,
      'file' => 'drealty_lead.lead.admin.inc',
      'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes'
    );
		
    return $items;
  }

}

function drealty_lead_add_page() {
  $item = menu_get_item();
  if ($content = system_admin_menu_block($item)) {
    $output = theme('admin_block_content', array('content' => $content));
  } else {
    $output = t('You do not have any administrative items.');
  }
  return $output;
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
    'id' => array('data' => t('ID'), 'field' => 'id'),
		'rets_id' => array('data' => t('MLS #'), 'field' => 'rets_id'),
		'name' => array('data' => t('Name'), 'field' => 'name'),
		'phone' => array('data' => t('Phone'), 'field' => 'phone'),
		'created' => array('data' => t('Created'), 'field' => 'created'),
    'operations' => array('data' => t('Operations')),
  );

  $query = db_select('drealty_lead', 'd')->extend('PagerDefault')->extend('TableSort');
  $result = $query
    ->fields('d', array('id', 'id'))
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

    $links = menu_contextual_links('drealty', 'admin/content/drealty-leads', array($lead->id));

    $options[$lead->id] = array(
      'id' => $lead->id,
			'rets_id' => $lead->field_drealty_lead_mls_id[LANGUAGE_NONE][0]['value'],
			'name' => $lead->field_drealty_lead_name[LANGUAGE_NONE][0]['value'],
			'phone' => $lead->field_drealty_lead_phone[LANGUAGE_NONE][0]['value'],
			'created' => date('F j, Y', $lead->created),
      'operations' => theme('links', array('links' => $links, 'attributes' => array('class' => array('links', 'inline', 'operations')))),
    );
  }
	
  $form['leads'] = array(
    '#type' => 'tableselect',
    '#header' => $header,
    '#options' => $options,
    '#empty' => t('No leads exist yet.'),
  );

  $form['pager'] = array('#theme' => 'pager');
  return $form;
}

function drealty_lead_form_wrapper($op, $lead) {
  return drupal_get_form('drealty_lead_edit_form', $op, $lead);
}

function drealty_lead_edit_form($form, &$form_state, $op, $lead) {

	$lead = end(entity_load('drealty_lead', array($lead->id)));
	
	if (!$lead) {
		$lead = new DrealtyLead;
	}
	
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
    '#value' => t('Save lead'),
    '#submit' => $submit + array('drealty_lead_edit_form_submit'),
  );

	if ($op != 'create') {
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
  field_attach_submit('drealty_lead', $lead, $form, $form_state);
  // save the listing
  $lead->save();
  // redirect back to the content list
  $form_state['redirect'] = 'admin/content/drealty-leads';
}

/**
 * Lead view page.
 */
function drealty_lead_view($entity, $view_mode = 'full', $langcode = NULL, $page = NULL) {
  $controller = entity_get_controller('drealty_lead');
  $content = $controller->view(array($entity->id => $entity), $view_mode, $langcode, $page);

  return $content;
}