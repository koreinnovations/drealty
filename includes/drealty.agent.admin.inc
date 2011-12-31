<?php

class DrealtyAgentUIController extends EntityDefaultUIController {

  public function hook_menu() {
    $id_count = count(explode('/', $this->path));
    $wildcard = isset($this->entityInfo['admin ui']['menu wildcard']) ? $this->entityInfo['admin ui']['menu wildcard'] : '%' . $this->entityType;

    $items[$this->path] = array(
        'title' => 'dRealty Agents',
        'page callback' => 'drupal_get_form',
        'page arguments' => array('drealty_agent_admin'),
        'access arguments' => array('access administration pages'),
        'type' => MENU_LOCAL_TASK | MENU_NORMAL_ITEM,
        'file' => 'drealty.agent.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
    );
    $items[$this->path . '/add'] = array(
        'title' => 'Add dRealty agent',
        'page callback' => 'drealty_agent_form_wrapper',
        'page arguments' => array(drealty_agent_create()),
        'access callback' => 'drealty_agent_access',
        'access arguments' => array('edit', $id_count + 1),
        'file' => 'drealty.agent.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
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
        'file' => 'drealty.agent.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
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
        'file' => 'drealty.agent.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes'
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

function drealty_agent_admin($form, $form_state) {

  if (isset($form_state['values']['operation']) && $form_state['values']['operation'] == 'delete') {
    return node_multiple_delete_confirm($form, $form_state, array_filter($form_state['values']['nodes']));
  }
  $form['filter'] = drealty_agent_filter_form();
  $form['#submit'][] = 'drealty_agent_filter_form_submit';
  $form['admin'] = drealty_agent_admin_overview();

  return $form;
}

function drealty_agent_admin_overview() {
  $header = array(
      'id' => array('data' => t('agent Id'), 'field' => 'id'),
      'agent_id' => array('data' => t('Agent MLS Id'), 'field' => 'agent_id'),
      'name' => array('data' => t('Name'), 'field' => 'last_name'),
      'email' => array('data' => t('Email')),
      'operations' => array('data' => t('Operations')),
  );

  $query = db_select('drealty_agent', 'd')->extend('PagerDefault')->extend('TableSort');

  drealty_agent_build_filter_query($query);

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
  // $form['#validate'][] = 'drealty_agent_edit_form_validate';
  return $form;
}

function drealty_agent_edit_form_submit($form, &$form_state) {
  // grab the agent object from the form state
  $agent = $form_state['agent'];
  // setup to save any fields from the field api
  field_attach_submit('drealty_agent', $agent, $form, $form_state);
  // save the agent
  $agent->save();
  // redirect back to the content list
  $form_state['redirect'] = 'admin/content/drealty_agents';
}

function drealty_agent_operations() {
  $operations = array(
      'refresh' => array(
          'label' => t('Refresh the selected agent'),
          'callback' => 'drealty_refresh_agent',
      ),
      'delete' => array(
          'label' => t('Delete the selected agent(s)'),
          'callback' => NULL,
      ),
  );
  return $operations;
}

/**
 * List node administration filters that can be applied.
 */
function drealty_agent_filters() {
  // Regular filters
  $filters['status'] = array(
      'title' => t('Office'),
      'options' => array(
          '[any]' => t('any'),
          'myoffice' => t('Belonging to my Office'),
      ),
  );
  return $filters;
}

/**
 * Apply filters for node administration filters based on session.
 *
 * @param $query
 *   A SelectQuery to which the filters should be applied.
 */
function drealty_agent_build_filter_query(SelectQueryInterface $query) {
  // Build query
  $filter_data = isset($_SESSION['drealty_agent_overview_filter']) ? $_SESSION['drealty_agent_overview_filter'] : array();
  foreach ($filter_data as $index => $filter) {
    list($key, $value) = $filter;
    switch ($key) {
      case 'status':
        
        // Note: no exploitable hole as $key/$value have already been checked when submitted
        $dc = new dRealtyConnection();
        $connections = $dc->FetchConnections();
        $ored = db_or();
        
        foreach ($connections as $connection) {
          $and = db_and()
            ->condition('conid', $connection->conid)
            ->condition('office_id', $connection->office_id);
          
          $ored->condition($and);
        }
        $query->condition($ored);
        break;
    }
  }
}

/**
 * Return form for node administration filters.
 */
function drealty_agent_filter_form() {
  $session = isset($_SESSION['drealty_agent_overview_filter']) ? $_SESSION['drealty_agent_overview_filter'] : array();
  $filters = drealty_agent_filters();

  $i = 0;
  $form['filters'] = array(
      '#type' => 'fieldset',
      '#title' => t('Show only items where'),
      '#theme' => 'exposed_filters__node',
  );
  foreach ($session as $filter) {
    list($type, $value) = $filter;
    if ($type == 'term') {
      // Load term name from DB rather than search and parse options array.
      $value = module_invoke('taxonomy', 'term_load', $value);
      $value = $value->name;
    } elseif ($type == 'language') {
      $value = $value == LANGUAGE_NONE ? t('Language neutral') : module_invoke('locale', 'language_name', $value);
    } else {
      $value = $filters[$type]['options'][$value];
    }
    $t_args = array('%property' => $filters[$type]['title'], '%value' => $value);
    if ($i++) {
      $form['filters']['current'][] = array('#markup' => t('and where %property is %value', $t_args));
    } else {
      $form['filters']['current'][] = array('#markup' => t('where %property is %value', $t_args));
    }
    if (in_array($type, array('type', 'language'))) {
      // Remove the option if it is already being filtered on.
      unset($filters[$type]);
    }
  }

  $form['filters']['status'] = array(
      '#type' => 'container',
      '#attributes' => array('class' => array('clearfix')),
      '#prefix' => ($i ? '<div class="additional-filters">' . t('and where') . '</div>' : ''),
  );
  $form['filters']['status']['filters'] = array(
      '#type' => 'container',
      '#attributes' => array('class' => array('filters')),
  );
  foreach ($filters as $key => $filter) {
    $form['filters']['status']['filters'][$key] = array(
        '#type' => 'select',
        '#options' => $filter['options'],
        '#title' => $filter['title'],
        '#default_value' => '[any]',
    );
  }

  $form['filters']['status']['actions'] = array(
      '#type' => 'actions',
      '#attributes' => array('class' => array('container-inline')),
  );
  $form['filters']['status']['actions']['submit'] = array(
      '#type' => 'submit',
      '#value' => count($session) ? t('Refine') : t('Filter'),
  );
  if (count($session)) {
    $form['filters']['status']['actions']['undo'] = array('#type' => 'submit', '#value' => t('Undo'));
    $form['filters']['status']['actions']['reset'] = array('#type' => 'submit', '#value' => t('Reset'));
  }

  drupal_add_js('misc/form.js');

  return $form;
}

/**
 * Process result from node administration filter form.
 */
function drealty_agent_filter_form_submit($form, &$form_state) {
  $filters = drealty_agent_filters();
  switch ($form_state['values']['op']) {
    case t('Filter'):
    case t('Refine'):
      // Apply every filter that has a choice selected other than 'any'.
      foreach ($filters as $filter => $options) {
        if (isset($form_state['values'][$filter]) && $form_state['values'][$filter] != '[any]') {
          // Flatten the options array to accommodate hierarchical/nested options.
          $flat_options = form_options_flatten($filters[$filter]['options']);
          // Only accept valid selections offered on the dropdown, block bad input.
          if (isset($flat_options[$form_state['values'][$filter]])) {
            $_SESSION['drealty_agent_overview_filter'][] = array($filter, $form_state['values'][$filter]);
          }
        }
      }
      break;
    case t('Undo'):
      array_pop($_SESSION['drealty_agent_overview_filter']);
      break;
    case t('Reset'):
      $_SESSION['drealty_agent_overview_filter'] = array();
      break;
  }
}
