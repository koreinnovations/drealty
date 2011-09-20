<?php

class DrealtyListingsUIController extends EntityDefaultUIController {

  public function hook_menu() {

    $id_count = count(explode('/', $this->path));
    $wildcard = isset($this->entityInfo['admin ui']['menu wildcard']) ? $this->entityInfo['admin ui']['menu wildcard'] : '%' . $this->entityType;

    $items[$this->path] = array(
        'title' => 'dRealty Listings',
        'page callback' => 'drealty_listings_admin',
        'access arguments' => array('access administration pages'),
        'type' => MENU_LOCAL_TASK | MENU_NORMAL_ITEM,
        'file' => 'drealty_listings.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']),
    );
    $items[$this->path . '/add'] = array(
        'title' => 'Add dRealty Listing',
        'page callback' => 'drealty_listing_form_wrapper',
        'page arguments' => array(drealty_listing_create(array('rets_imported' => FALSE))),
        'access callback' => 'drealty_listing_access',
        'access arguments' => array('edit', $id_count + 1),
        'file' => 'drealty_listings.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']),
        'type' => MENU_LOCAL_ACTION,
    );
    // Loading and editing model entities
    $items[$this->path . '/listing/' . $wildcard] = array(
        'title' => t('Edit Listing'),
        'page callback' => 'drealty_listing_form_wrapper',
        'page arguments' => array($id_count + 1),
        'access callback' => 'drealty_listing_access',
        'access arguments' => array('edit', $id_count + 1),
        'weight' => 0,
        'context' => MENU_CONTEXT_PAGE | MENU_CONTEXT_INLINE,
        'file' => 'drealty_listings.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module'])
    );
    $items[$this->path . '/listing/' . $wildcard . '/edit'] = array(
        'title' => 'Edit',
        'type' => MENU_DEFAULT_LOCAL_TASK,
        'weight' => -10,
        'context' => MENU_CONTEXT_PAGE | MENU_CONTEXT_INLINE,
    );

    $items[$this->path . '/listing/' . $wildcard . '/delete'] = array(
        'title' => 'Delete',
        'page callback' => 'model_delete_form_wrapper',
        'page arguments' => array($id_count + 1),
        'access callback' => 'drealty_listing_access',
        'access arguments' => array('edit', $id_count + 1),
        'type' => MENU_LOCAL_TASK,
        'context' => MENU_CONTEXT_INLINE,
        'weight' => 10,
        'file' => 'drealty_listings.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module'])
    );


    $items['listings/' . $wildcard] = array(
        'title callback' => 'drealty_listings_page_title',
        'title arguments' => array(1),
        'page callback' => 'drealty_listings_page_view',
        'page arguments' => array(1),
        'access callback' => 'drealty_listing_access',
        'access arguments' => array('view', 1),
        'type' => MENU_CALLBACK,
    );

    return $items;
  }

}

function drealty_listings_admin($type = 'new') {
  $edit = $_POST;
  if (isset($edit['operation']) && ($edit['operation'] == 'delete') && isset($edit['listings']) && $edit['listings']) {
    return drupal_get_form('drealty_listings_multiple_delete_confirm');
  } else {
    return drupal_get_form('drealty_listings_admin_overview', $type);
  }
}

function drealty_listings_admin_overview($form, &$form_state, $arg) {
  $header = array(
      'listing_id' => array('data' => t('Listing Id'), 'field' => 'id'),
      'mls_id' => array('data' => t('MLS #'), 'field' => 'listing_id'),
      'address' => array('data' => t('Address')),
      'operations' => array('data' => t('Operations')),
  );

  $query = db_select('drealty_listings', 'd')->extend('PagerDefault')->extend('TableSort');
  $result = $query
    ->fields('d', array('id', 'listing_id'))
    ->limit(50)
    ->orderByHeader($header)
    ->execute();

  $lids = array();
  foreach ($result as $row) {
    $lids[] = $row->id;
  }

  $listings = entity_load('drealty_listings', $lids);

  $options = array();

  foreach ($listings as $listing) {
    $address = "{$listing->street_number} {$listing->street_dir_prefix} {$listing->street_name} {$listing->street_dir_suffix} {$listing->street_suffix}, {$listing->city}, {$listing->state_or_province} {$listing->postal_code}";
    $address = str_replace("  ", " ", $address);
    $links = menu_contextual_links('model', 'admin/content/listings/listing', array($listing->listing_id));
    $options[$listing->id] = array(
        'listing_id' => $listing->id,
        'mls_id' => array('data' => array('#type' => 'link', '#title' => $listing->listing_id, '#href' => 'listings/' . $listing->listing_id)),
        'address' => $address,
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

function drealty_listing_form_wrapper($listing) {
  return drupal_get_form('drealty_listings_edit_form', $listing);
}

function drealty_listings_edit_form($form, &$form_state, $listing) {

  $form_state['listing'] = $listing;


  if ($listing->rets_imported) {
    $address = "{$listing->street_number} {$listing->street_dir_prefix} {$listing->street_name} {$listing->street_dir_suffix} {$listing->street_suffix}, {$listing->city}, {$listing->state_or_province} {$listing->postal_code}";
    $address = str_replace("  ", " ", $address);
    $form['address'] = array('#markup' => "<div><strong>Address: </strong>$address</div>");
    $form['mlsid'] = array('#markup' => "<div><strong>MSL#: </strong> {$listing->listing_id}</div>");
    $form['mlskey'] = array('#markup' => "<div><strong>MLS Key: </strong> {$listing->listing_key}</div>");
    $form['price'] = array('#markup' => "<div><strong>Price: </strong> {$listing->list_price}</div>");
  }



  field_attach_form('drealty_listings', $listing, $form, $form_state);

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

  if (!empty($listing->name)) {
    $form['actions']['delete'] = array(
        '#type' => 'submit',
        '#value' => t('Delete listing'),
        '#suffix' => l(t('Cancel'), 'admin/content/listings'),
        '#submit' => $submit + array('drealty_listing_form_submit_delete'),
        '#weight' => 45,
    );
  }

  // We append the validate handler to #validate in case a form callback_wrapper
  // is used to add validate handlers earlier.
  $form['#validate'][] = 'drealty_listing_edit_form_validate';
  return $form;
}