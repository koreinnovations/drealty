<?php

class DrealtyAgentUIController extends EntityDefaultUIController {

  public function hook_menu() {

    $id_count = count(explode('/', $this->path));
    $wildcard = isset($this->entityInfo['admin ui']['menu wildcard']) ? $this->entityInfo['admin ui']['menu wildcard'] : '%' . $this->entityType;

    $items[$this->path] = array(
        'title' => 'dRealty Agents',
        'page callback' => 'drealty_agent_admin',
        'access arguments' => array('access administration pages'),
        'type' => MENU_LOCAL_TASK | MENU_NORMAL_ITEM,
        'file' => 'drealty_agent.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']),
    );
    $items[$this->path . '/add'] = array(
        'title' => 'Add dRealty agent',
        'page callback' => 'drealty_agent_form_wrapper',
        'page arguments' => array(drealty_agent_create()),
        'access callback' => 'drealty_agent_access',
        'access arguments' => array('edit', $id_count + 1),
        'file' => 'drealty_agent.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']),
        'type' => MENU_LOCAL_ACTION,
    );
    // Loading and editing model entities
    $items[$this->path . '/' . $wildcard] = array(
        'title' => t('Edit agent'),
        'page callback' => 'drealty_agent_form_wrapper',
        'page arguments' => array('edit', $id_count),
        'access callback' => 'drealty_agent_access',
        'access arguments' => array('edit', $id_count),
        'weight' => 0,
        'context' => MENU_CONTEXT_PAGE | MENU_CONTEXT_INLINE,
        'file' => 'drealty_agent.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module'])
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
        'page arguments' => array('delete', $id_count),
        'access callback' => 'drealty_agent_access',
        'access arguments' => array('delete', $id_count),
        'type' => MENU_LOCAL_TASK,
        'context' => MENU_CONTEXT_INLINE,
        'weight' => 10,
        'file' => 'drealty_agent.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module'])
    );

    $items[$this->path . '/' . $wildcard . '/view'] = array(
        'title' => t('View'),
        'page callback' => 'drealty_agent_page_view',
        'page arguments' => array($id_count),
        'access callback' => 'drealty_agent_access',
        'access arguments' => array('view', $id_count),
        'type' => MENU_LOCAL_TASK,
        'context' => MENU_CONTEXT_INLINE,
        'weight' => -11,
    );
    $items['agents/' . $wildcard] = array(
        'title callback' => 'drealty_agent_page_title',
        'title arguments' => array(1),
        'page callback' => 'drealty_agent_page_view',
        'page arguments' => array(1),
        'access callback' => 'drealty_agent_access',
        'access arguments' => array('view', 1),
        'type' => MENU_CALLBACK,
    );

    return $items;
  }

}

function drealty_agent_admin($type = 'new') {
  $edit = $_POST;
  if (isset($edit['operation']) && ($edit['operation'] == 'delete') && isset($edit['agents']) && $edit['agents']) {
    return drupal_get_form('drealty_agent_multiple_delete_confirm');
  } else {
    return drupal_get_form('drealty_agent_admin_overview', $type);
  }
}

function drealty_agent_admin_overview($form, &$form_state, $arg) {
  $header = array(
      'id' => array('data' => t('agent Id'), 'field' => 'id'),
      'agent_id' => array('data' => t('Agent MLS Id'), 'field' => 'agent_id'),
      'name' => array('data' => t('Name'), 'field' => 'last_name'),
      'email' => array('data' => t('Email')),
      'operations' => array('data' => t('Operations')),
  );

  $query = db_select('drealty_agent', 'd')->extend('PagerDefault')->extend('TableSort');
  $result = $query
    ->fields('d', array('id', 'agent_id'))
    ->limit(50)
    ->orderByHeader($header)
    ->execute();

  $lids = array();
  foreach ($result as $row) {
    $lids[] = $row->id;
  }

  $agents = entity_load('drealty_agent', $lids);

  $options = array();

  foreach ($agents as $agent) {

    $links = menu_contextual_links('drealty', 'admin/content/drealty_agents', array("{$agent->first_name}_{$agent->last_name}"));
    $options[$agent->id] = array(
        'id' => $agent->id,
        'agent_id' => $agent->agent_id,
        'name' => "{$agent->first_name} {$agent->last_name}",
        'email' => !empty($agent->email) ? l($agent->email, "mailto:$agent->email", array('absolute' => TRUE)) : "n/a",
        'operations' => theme('links', array('links' => $links, 'attributes' => array('class' => array('links', 'inline', 'operations')))),
    );
  }
  $form['agents'] = array(
      '#type' => 'tableselect',
      '#header' => $header,
      '#options' => $options,
      '#empty' => t('No agents available.'),
  );

  $form['pager'] = array('#theme' => 'pager');
  return $form;
}

function drealty_agent_form_wrapper($op, $agent) {
  return drupal_get_form('drealty_agent_edit_form', $op, $agent);
}

function drealty_agent_edit_form($form, &$form_state, $op, $agent) {

  $form_state['agent'] = $agent;


  if ($agent->hash) {
    $form['name'] = array('#markup' => "<div><strong>Name: </strong>{$agent->first_name} {$agent->last_name}</div>");
    $form['mlsid'] = array('#markup' => "<div><strong>MSL Id: </strong> {$agent->agent_id}</div>");
    $form['phone'] = array('#markup' => "<div><strong>Office Phone: </strong> {$agent->phone_office} {$agent->phone_office_ext}</div>");
    $form['cell'] = array('#markup' => "<div><strong>Cell: </strong> {$agent->phone_cell}</div>");
  }

  field_attach_form('drealty_agent', $agent, $form, $form_state);

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
      '#value' => t('Save agent'),
      '#submit' => $submit + array('drealty_agent_edit_form_submit'),
  );

  if (!empty($agent->name)) {
    $form['actions']['delete'] = array(
        '#type' => 'submit',
        '#value' => t('Delete agent'),
        '#suffix' => l(t('Cancel'), 'admin/content/agents'),
        '#submit' => $submit + array('drealty_agent_form_submit_delete'),
        '#weight' => 45,
    );
  }

  // We append the validate handler to #validate in case a form callback_wrapper
  // is used to add validate handlers earlier.
  $form['#validate'][] = 'drealty_agent_edit_form_validate';
  return $form;
}