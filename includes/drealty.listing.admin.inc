<?php

class DrealtyListingUIController extends EntityDefaultUIController {

  public function hook_menu() {

    $id_count = count(explode('/', $this->path));
    $wildcard = isset($this->entityInfo['admin ui']['menu wildcard']) ? $this->entityInfo['admin ui']['menu wildcard'] : '%' . $this->entityType;

    $items[$this->path] = array(
        'title' => 'dRealty Listings',
        'page callback' => 'drealty_listing_admin',
        'access arguments' => array('access administration pages'),
        'type' => MENU_LOCAL_TASK | MENU_NORMAL_ITEM,
        'file' => 'drealty.listing.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
    );
    $items[$this->path . '/add'] = array(
        'title' => 'Add dRealty Listing',
        'page callback' => 'drealty_listing_form_wrapper',
        'page arguments' => array('add', drealty_listing_create(array('rets_imported' => FALSE))),
        'access callback' => 'drealty_listing_access',
        'access arguments' => array('add'),
        'file' => 'drealty.listing.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes',
        'type' => MENU_LOCAL_ACTION,
    );
    // Loading and editing model entities
    $items[$this->path . '/' . $wildcard] = array(
        'title' => t('Edit Listing'),
        'page callback' => 'drealty_listing_form_wrapper',
        'page arguments' => array('edit', $id_count),
        'access callback' => 'drealty_listing_access',
        'access arguments' => array('edit', $id_count),
        'weight' => 0,
        'context' => MENU_CONTEXT_PAGE | MENU_CONTEXT_INLINE,
        'file' => 'drealty.listing.admin.inc',
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
        'page callback' => 'drealty_listing_form_wrapper',
        'page arguments' => array('delete', $id_count),
        'access callback' => 'drealty_listing_access',
        'access arguments' => array('delete', $id_count),
        'type' => MENU_LOCAL_TASK,
        'context' => MENU_CONTEXT_INLINE,
        'weight' => 10,
        'file' => 'drealty.listing.admin.inc',
        'file path' => drupal_get_path('module', $this->entityInfo['module']) . '/includes'
    );


    $items['listings/' . $wildcard] = array(
        'title callback' => 'drealty_listing_page_title',
        'title arguments' => array(1),
        'page callback' => 'drealty_listing_page_view',
        'page arguments' => array(1),
        'access callback' => 'drealty_listing_access',
        'access arguments' => array('view', 1),
        'type' => MENU_CALLBACK,
    );

    return $items;
  }

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
      'listing_id' => array('data' => t('Listing Id'), 'field' => 'id'),
      'mls_id' => array('data' => t('MLS #'), 'field' => 'listing_id'),
      'address' => array('data' => t('Address')),
      'operations' => array('data' => t('Operations')),
  );

  $query = db_select('drealty_listing', 'd')->extend('PagerDefault')->extend('TableSort');
  $result = $query
    ->fields('d', array('id', 'listing_id'))
    ->limit(50)
    ->orderByHeader($header)
    ->execute();

  $lids = array();
  foreach ($result as $row) {
    $lids[] = $row->id;
  }

  $listings = entity_load('drealty_listing', $lids);

  $options = array();

  foreach ($listings as $listing) {
    $address = "{$listing->street_number} {$listing->street_dir_prefix} {$listing->street_name} {$listing->street_dir_suffix} {$listing->street_suffix}, {$listing->city}, {$listing->state_or_province} {$listing->postal_code}";
    $address = str_replace("  ", " ", $address);
    $links = menu_contextual_links('drealty', 'admin/content/drealty_listings', array($listing->listing_id));

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

function drealty_listing_form_wrapper($op, $listing) {
  return drupal_get_form('drealty_listing_edit_form', $op, $listing);
}

function drealty_listing_edit_form($form, &$form_state, $op, $listing) {

  $form_state['listing'] = $listing;


  if ($listing->rets_imported) {

    $path = drupal_get_path('module', 'drealty') . "/css/drealty.admin.css";
    drupal_add_css($path, array('type' => 'file'));

    $address = "{$listing->street_number} {$listing->street_dir_prefix} {$listing->street_name} {$listing->street_dir_suffix} {$listing->street_suffix}, {$listing->city}, {$listing->state_or_province} {$listing->postal_code}";
    $address = str_replace("  ", " ", $address);
    $price = number_format($listing->list_price);
    $form['address'] = array('#markup' => "<div><strong>Address: </strong>$address</div>", '#prefix' => '<div class="section">');
    $form['mlsid'] = array('#markup' => "<div><strong>MSL#: </strong> {$listing->listing_id}</div>");
    $form['mlskey'] = array('#markup' => "<div><strong>MLS Key: </strong> {$listing->listing_key}</div>");
    $form['price'] = array('#markup' => "<div><strong>Price: </strong>$$price</div>", '#suffix' => '</div>');
  } else {
    $listing_info = entity_get_property_info('drealty_listing');
    $props = $listing_info['properties'];

    $pattern = '/(.*?[a-z]{1})([A-Z]{1}.*?)/';
    $replace = '${1} ${2}';

    foreach ($props as &$prop) {
      $prop['label'] = preg_replace($pattern, $replace, $prop['label']);
    }

    $form['sections']['required'] = array('#type' => 'fieldset', '#title' => 'Required', '#collapsible' => TRUE, '#collapsed' => FALSE);
    $form['sections']['general'] = array('#type' => 'fieldset', '#title' => 'General', '#collapsible' => TRUE, '#collapsed' => TRUE);
    $form['sections']['Listing:Location:Address'] = array('#type' => 'fieldset', '#title' => 'Address', '#collapsible' => TRUE, '#collapsed' => TRUE);
    $form['sections']['Listing:Location:School'] = array('#type' => 'fieldset', '#title' => 'School', '#collapsible' => TRUE, '#collapsed' => TRUE);
    $form['sections']['Listing:Location:Area'] = array('#type' => 'fieldset', '#title' => 'Area', '#collapsible' => TRUE, '#collapsed' => TRUE);
    $form['sections']['Listing:Location:GIS'] = array('#type' => 'fieldset', '#title' => 'GIS / Mapping', '#collapsible' => TRUE, '#collapsed' => TRUE);
    $form['sections']['Listing:AgentOffice:ListAgent'] = array('#type' => 'fieldset', '#title' => 'Listing Agent', '#collapsible' => TRUE, '#collapsed' => TRUE);
    $form['sections']['Listing:AgentOffice:ListOffice'] = array('#type' => 'fieldset', '#title' => 'Listing Office', '#collapsible' => TRUE, '#collapsed' => TRUE);
    $form['sections']['Listing:AgentOffice:CoListAgent'] = array('#type' => 'fieldset', '#title' => 'Co-Listing Agent', '#collapsible' => TRUE, '#collapsed' => TRUE);
    $form['sections']['Listing:AgentOffice:CoListOffice'] = array('#type' => 'fieldset', '#title' => 'Co-Listing Office', '#collapsible' => TRUE, '#collapsed' => TRUE);
    $form['sections']['Listing:Property'] = array('#type' => 'fieldset', '#title' => 'Property Information', '#collapsible' => TRUE, '#collapsed' => TRUE);
    $form['sections']['Listing:Property:Structure'] = array('#type' => 'fieldset', '#title' => 'Structure', '#collapsible' => TRUE, '#collapsed' => TRUE);
    $form['sections']['Listing:Property:Rooms'] = array('#type' => 'fieldset', '#title' => 'Individual Rooms', '#collapsible' => TRUE, '#collapsed' => TRUE);


    asort($props);
    foreach ($props as $key => $property) {
      if (isset($property['group']) && $property['group'] != 'hidden') {
        
        $type = 'textfield';
        switch($property['schema_type']) {
          case 'text':
            $type = 'textarea';
            break;
          default:
            $type = 'textfield';
        }
        
        $form['sections'][$property['group']][$key] = array(
            '#type' => $type,
            '#title' => $property['label'],
            '#description' => $property['description'],
            '#default_value' => isset($listing->{$key})?$listing->{$key}:NULL,
        );
      }
    }
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
        '#suffix' => l(t('Cancel'), 'admin/content/listings'),
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