<?php

class DrealtyListingUIController extends EntityDefaultUIController {

  public function hook_menu() {

    $id_count = count(explode('/', $this->path));
    $wildcard = isset($this->entityInfo['admin ui']['menu wildcard']) ? $this->entityInfo['admin ui']['menu wildcard'] : '%' . $this->entityType;

    $items[$this->path] = array(
      'title' => 'Drealty Listings',
      'description' => 'Manage Drealty Listing Content',
      'page callback' => 'drealty_listing_admin',
      'access arguments' => array('access drealty admin'),
      'type' => MENU_LOCAL_TASK | MENU_NORMAL_ITEM,
      'file' => 'drealty.listing.admin.inc',
      'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
    );

    $items['drealty_listing/add'] = array(
      'title' => 'Add dRealty Listing',
      'page callback' => 'drealty_listing_add_page',
      'access arguments' => array('create drealty listing'),
      'file' => 'drealty.listing.admin.inc',
      'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
      'type' => MENU_NORMAL_ITEM,
    );
    foreach (drealty_listings_get_types() as $type) {
      $type_url_str = str_replace('_', '-', $type->type);
      $items['drealty_listing/add/' . $type_url_str] = array(
        'title' => $type->label,
        'title callback' => 'check_plain',
        'page callback' => 'drealty_listing_form_wrapper',
        'page arguments' => array('add', drealty_listing_create(array('rets_imported' => FALSE, 'type' => $type->type))),
        'access arguments' => array('create drealty listing'),
        'file' => 'drealty.listing.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
        'type' => MENU_NORMAL_ITEM,
      );
    }

    $items['drealty_listing/' . $wildcard] = array(
      'title callback' => 'drealty_listing_page_title',
      'title arguments' => array(1),
      'page callback' => 'drealty_listing_view',
      'page arguments' => array(1),
      'access callback' => 'drealty_access',
      'access arguments' => array('view', 1),
      'type' => MENU_CALLBACK,
    );

    $items['drealty_listing/' . $wildcard . '/view'] = array(
      'title' => 'View',
      'type' => MENU_DEFAULT_LOCAL_TASK,
      'weight' => -10,
    );

    // Loading and editing model entities
    $items['drealty_listing/' . $wildcard . '/edit'] = array(
      'title' => 'Edit',
      'page callback' => 'drealty_listing_form_wrapper',
      'page arguments' => array('edit', 1),
      'access callback' => 'drealty_access',
      'access arguments' => array('edit', 1),
      'weight' => 0,
      'context' => MENU_CONTEXT_PAGE | MENU_CONTEXT_INLINE,
      'type' => MENU_LOCAL_TASK,
      'file' => 'drealty.listing.admin.inc',
      'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes'
    );

    $items['drealty_listing/' . $wildcard . '/delete'] = array(
      'title' => 'Delete',
      'page callback' => 'drealty_listing_form_wrapper',
      'page arguments' => array('delete', 1),
      'access callback' => 'drealty_access',
      'access arguments' => array('delete', 1),
      'type' => MENU_LOCAL_TASK,
      'context' => MENU_CONTEXT_INLINE,
      'weight' => 10,
      'file' => 'drealty.listing.admin.inc',
      'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes'
    );

    if (module_exists('devel')) {
      $items['drealty_listing/' . $wildcard . '/devel'] = array(
        'title' => 'Devel',
        'page callback' => 'devel_load_object',
        'page arguments' => array('node', 1),
        'access arguments' => array('access devel information'),
        'type' => MENU_LOCAL_TASK,
        'file' => 'devel.pages.inc',
        'file path' => drupal_get_path('module', 'devel'),
        'weight' => 100,
      );
      $items['drealty_listing/' . $wildcard . '/devel/load'] = array(
        'title' => 'Load',
        'type' => MENU_DEFAULT_LOCAL_TASK,
      );
      $items['drealty_listing/' . $wildcard . '/devel/render'] = array(
        'title' => 'Render',
        'page callback' => 'devel_render_object',
        'page arguments' => array('drealty_listing', 1),
        'access arguments' => array('access devel information'),
        'file' => 'devel.pages.inc',
        'file path' => drupal_get_path('module', 'devel'),
        'type' => MENU_LOCAL_TASK,
        'weight' => 100,
      );
    }

    $items['drealty_listing/' . $wildcard . '/rets_refresh'] = array(
      'title' => 'Update RETS Data',
      'page callback' => 'drupal_get_form',
      'page arguments' => array('drealty_update_single_listing', 1),
      'access arguments' => array('access drealty admin'),
      'type' => MENU_LOCAL_TASK,
      'context' => MENU_CONTEXT_INLINE | MENU_CONTEXT_PAGE,
      'weight' => 99,
    );

    return $items;
  }

}

function drealty_listing_add_page() {
  $item = menu_get_item();
  if ($content = system_admin_menu_block($item)) {
    $output = theme('admin_block_content', array('content' => $content));
  } else {
    $output = t('You do not have any administrative items.');
  }
  return $output;
}

function drealty_listing_admin($type = 'new') {
  $edit = $_POST;
  if (isset($edit['operation']) && ($edit['operation'] == 'delete') && isset($edit['listings']) && $edit['listings']) {
    return drupal_get_form('drealty_listing_multiple_delete_confirm');
  } else {
    return drupal_get_form('drealty_listing_admin_overview', $type);
  }
}

function drealty_listing_admin_overview($form, &$form_state, $arg) {
  $header = array(
    'id' => array('data' => t('Listing Id'), 'field' => 'id'),
    'mls_id' => array('data' => t('MLS #'), 'field' => 'rets_id'),
    'title' => array('data' => t('Title'), 'field' => 'label'),
    'type' => array('data' => t('Type'), 'field' => 'type'),
    'updated' => array('data' => t('Changed'), 'field' => 'created'),
    'operations' => array('data' => t('Operations')),
  );

  $query = db_select('drealty_listing', 'd')->extend('PagerDefault')->extend('TableSort');
  $result = $query
    ->fields('d', array('id', 'rets_id', 'label'))
    ->limit(10)
    ->orderByHeader($header)
    ->execute();

  $lids = array();
  foreach ($result as $row) {
    $lids[] = $row->id;
  }

  $listings = entity_load('drealty_listing', $lids);

  $options = array();

  foreach ($listings as $listing) {

    $links = menu_contextual_links('drealty', 'drealty_listing', array($listing->id));

    $options[$listing->id] = array(
      'id' => $listing->id,
      'mls_id' => array('data' => array('#type' => 'link', '#title' => $listing->rets_id, '#href' => 'drealty_listing/' . $listing->id)),
      'title' => $listing->label,
      'type' => $listing->type,      
      'updated' => format_date($listing->changed, 'short'),
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

function drealty_listing_form_wrapper($op, $listing) {
  return drupal_get_form('drealty_listing_edit_form', $op, $listing);
}

function drealty_listing_edit_form($form, &$form_state, $op, $listing) {

  $form_state['listing'] = $listing;


  if ($listing->rets_imported) {
    
  }

  field_attach_form('drealty_listing', $listing, $form, $form_state);

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
    '#submit' => $submit + array('drealty_listing_edit_form_submit'),
  );

  if (!empty($listing->name) && !$listing->rets_imported) {
    $form['actions']['delete'] = array(
      '#type' => 'submit',
      '#value' => t('Delete listing'),
      '#suffix' => l(t('Cancel'), 'admin/content/drealty_listings'),
      '#submit' => $submit + array('drealty_listing_form_submit_delete'),
      '#weight' => 45,
    );
  }

  return $form;
}

function drealty_listing_edit_form_submit($form, &$form_state) {
  // grab the listing
  $listing = $form_state['listing'];
  // attach any fields from the field api
  field_attach_submit('drealty_listing', $listing, $form, $form_state);
  // save the listing
  $listing->save();
  // redirect back to the content list
  $form_state['redirect'] = 'admin/content/drealty_listings';
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
          '#title' => t('Property Type Label'),
          '#description' => t('The human-readable name of this property type.'),
          '#required' => TRUE,
          '#size' => 80,
          '#default_value' => '',
        );
        $form['type'] = array(
          '#type' => 'machine_name',
          '#title' => t('Property Type Name'),
          '#machine_name' => array(
            'exists' => 'drealty_listing_type_name_exists',
            'source' => array('label'),
          ),
          '#description' => t('A unique machine-readable name for this Property Type. It must only contain lowercase letters, numbers, and underscores.'),
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
function drealty_listing_type_form($form, &$form_state, DrealtyListingType $drealty_listing_type, $op = 'edit') {

  if ($op == 'clone') {
    $drealty_listing_type->label .= ' (cloned)';
    $drealty_listing_type->type = '';
  }

  $form['label'] = array(
    '#title' => t('Label'),
    '#type' => 'textfield',
    '#default_value' => $drealty_listing_type->label,
    '#description' => t('The human-readable name of this drealty listing type.'),
    '#required' => TRUE,
    '#size' => 30,
  );
  // Machine-readable type name.
  $form['type'] = array(
    '#type' => 'machine_name',
    '#default_value' => isset($drealty_listing_type->type) ? $drealty_listing_type->type : '',
    '#maxlength' => 32,
    '#disabled' => $drealty_listing_type->isLocked() && $op != 'clone',
    '#machine_name' => array(
      'exists' => 'drealty_listings_get_types',
      'source' => array('label'),
    ),
    '#description' => t('A unique machine-readable name for this drealty listing type. It must only contain lowercase letters, numbers, and underscores.'),
  );


  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save drealty listing type'),
    '#weight' => 40,
  );

  if (!$drealty_listing_type->isLocked() && $op != 'add' && $op != 'clone') {
    $form['actions']['delete'] = array(
      '#type' => 'submit',
      '#value' => t('Delete listing type'),
      '#weight' => 45,
      '#limit_validation_errors' => array(),
      '#submit' => array('drealty_listing_type_form_submit_delete')
    );
  }
  return $form;
}

/**
 * Form API submit callback for the type form.
 */
function drealty_listing_type_form_submit(&$form, &$form_state) {
  $drealty_listing_type = entity_ui_form_submit_build_entity($form, $form_state);
  // Save and go back.
  $drealty_listing_type->save();
  $form_state['redirect'] = 'admin/drealty/drealty_listings';
}

/**
 * Form API submit callback for the delete button.
 */
function drealty_listing_type_form_submit_delete(&$form, &$form_state) {
  $form_state['redirect'] = 'admin/drealty/drealty_listings/manage/' . $form_state['drealty_listing_type']->type . '/delete';
}

