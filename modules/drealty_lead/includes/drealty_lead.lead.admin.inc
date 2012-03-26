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
      'file' => 'drealty.lead.admin.inc',
      'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
    );
    $items[$this->path . '/add'] = array(
      'title' => 'Add dRealty Lead',
      'page callback' => 'drealty_lead_form_wrapper',
      'page arguments' => array('add', drealty_lead_create(array('rets_imported' => FALSE))),
      'access arguments' => array('create drealty listing'),
      'file' => 'drealty.lead.admin.inc',
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
      'file' => 'drealty.lead.admin.inc',
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

/**
 * UI Controller
 */
class DrealtyListingTypeUIController extends EntityDefaultUIController {

  /**
   * Overrides hook_menu() defaults.
   */
  public function hook_menu() {
    $items = parent::hook_menu();
    $items[$this->path]['description'] = 'Manage drealty listings, including fields.';
    return $items;
  }

  public function operationForm($form, &$form_state, $entity, $op) {
    switch ($op) {
      case 'revert':
        $label = entity_label($this->entityType, $entity);
        $confirm_question = t('Are you sure you want to revert the %entity %label?', array('%entity' => $this->entityInfo['label'], '%label' => $label));
        return confirm_form($form, $confirm_question, $this->path);

      case 'delete':
        $label = entity_label($this->entityType, $entity);
        $confirm_question = t('Are you sure you want to delete the %entity %label?', array('%entity' => $this->entityInfo['label'], '%label' => $label));
        return confirm_form($form, $confirm_question, $this->path);

      case 'export':
        if (!empty($this->entityInfo['exportable'])) {
          $export = entity_export($this->entityType, $entity);
          $form['export'] = array(
            '#type' => 'textarea',
            '#title' => t('Export'),
            '#description' => t('For importing copy the content of the text area and paste it into the import page.'),
            '#rows' => 25,
            '#default_value' => $export,
          );
          return $form;
        }

      case 'import':
        $form['label'] = array(
          '#type' => 'textfield',
          '#title' => t('Lead Type Label'),
          '#description' => t('The human-readable name of this lead type.'),
          '#required' => TRUE,
          '#size' => 80,
          '#default_value' => '',
        );
        $form['type'] = array(
          '#type' => 'machine_name',
          '#title' => t('Lead Type Name'),
          '#machine_name' => array(
            'exists' => 'drealty_lead_type_name_exists',
            'source' => array('label'),
          ),
          '#description' => t('A unique machine-readable name for this Lead Type. It must only contain lowercase letters, numbers, and underscores.'),
          '#size' => 80,
          '#default_value' => '',
        );        
        
        $form['import'] = array(
          '#type' => 'textarea',
          '#title' => t('Import'),
          '#description' => t('Paste an exported %entity_type here.', array('%entity_type' => $this->entityInfo['label'])),
          '#rows' => 20,
        );
        $form['submit'] = array(
          '#type' => 'submit',
          '#value' => t('Import'),
        );
        return $form;
    }
    drupal_not_found();
    exit;
  }

  /**
   * Operation form validation callback.
   */
  public function operationFormValidate($form, &$form_state) {
    if ($form_state['op'] == 'import') {

      if ($entity = entity_import($this->entityType, $form_state['values']['import'])) {
        // Store the successfully imported entity in $form_state.
        $form_state[$this->entityType] = $entity;
        $form_state[$this->entityType]->type = $form_state['values']['type'];
        $form_state[$this->entityType]->label = $form_state['values']['label'];

        // Check for existing entities with the same identifier.
        $id = entity_id($this->entityType, $entity);
        $entities = entity_load($this->entityType, array($id));
        if (!empty($entities)) {
          $label = entity_label($this->entityType, $entity);
          $vars = array('%entity' => $this->entityInfo['label'], '%label' => $label);
          form_set_error('import', t('Import of %entity %label failed, a %entity with the same machine name already exists. Check the overwrite option to replace it.', $vars));
        }
      } else {
        form_set_error('import', t('Import failed.'));
      }
    }
  }

}

/**
 * Generates the listing type editing form.
 */
function drealty_lead_type_form($form, &$form_state, DrealtyLeadType $drealty_lead_type, $op = 'edit') {

  if ($op == 'clone') {
    $drealty_lead_type->label .= ' (cloned)';
    $drealty_lead_type->type = '';
  }

  $form['label'] = array(
    '#title' => t('Label'),
    '#type' => 'textfield',
    '#default_value' => $drealty_lead_type->label,
    '#description' => t('The human-readable name of this drealty lead type.'),
    '#required' => TRUE,
    '#size' => 30,
  );
  // Machine-readable type name.
  $form['type'] = array(
    '#type' => 'machine_name',
    '#default_value' => isset($drealty_lead_type->type) ? $drealty_lead_type->type : '',
    '#maxlength' => 32,
    '#disabled' => $drealty_lead_type->isLocked() && $op != 'clone',
    '#machine_name' => array(
      'exists' => 'drealty_leads_get_types',
      'source' => array('label'),
    ),
    '#description' => t('A unique machine-readable name for this drealty lead type. It must only contain lowercase letters, numbers, and underscores.'),
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save drealty listing type'),
    '#weight' => 40,
  );

  if (!$drealty_lead_type->isLocked() && $op != 'add' && $op != 'clone') {
    $form['actions']['delete'] = array(
      '#type' => 'submit',
      '#value' => t('Delete listing type'),
      '#weight' => 45,
      '#limit_validation_errors' => array(),
      '#submit' => array('drealty_lead_type_form_submit_delete')
    );
  }
  return $form;
}

/**
 * Form API submit callback for the type form.
 */
function drealty_lead_type_form_submit(&$form, &$form_state) {
  $drealty_lead_type = entity_ui_form_submit_build_entity($form, $form_state);
  // Save and go back.
  $drealty_lead_type->save();
  $form_state['redirect'] = 'admin/drealty/drealty_leads';
}

/**
 * Form API submit callback for the delete button.
 */
function drealty_lead_type_form_submit_delete(&$form, &$form_state) {
  $form_state['redirect'] = 'admin/drealty/drealty_leads/manage/' . $form_state['drealty_lead_type']->type . '/delete';
}


