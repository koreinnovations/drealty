<?php

/**
 * @file
 * Sample ctools context type plugin that shows how to create a context from an arg.
 *
 */
/**
 * Plugins are described by creating a $plugin array which will be used
 * by the system that includes this file.
 */
$plugin = array(
    'title' => t("Listing Context"),
    'description' => t('A single "simplecontext" context, or data element.'),
    'context' => 'drealty_context_create_listingcontext', // func to create context
    'context name' => 'listingcontext',
    'settings form' => 'listingcontext_settings_form',
    'keyword' => 'listingcontext',
     'defaults' => array('id' => ''),
    // Provides a list of items which are exposed as keywords.
    'convert list' => 'listingcontext_convert_list',
    // Convert keywords into data.
    'convert' => 'listingcontext_convert',
    'placeholder form' => array(
        '#type' => 'textfield',
        '#description' => t('Enter some data to represent this "simplecontext".'),
    ),
);

/**
 * Create a context, either from manual configuration or from an argument on the URL.
 *
 * @param $empty
 *   If true, just return an empty context.
 * @param $data
 *   If from settings form, an array as from a form. If from argument, a string.
 * @param $conf
 *   TRUE if the $data is coming from admin configuration, FALSE if it's from a URL arg.
 *
 * @return
 *   a Context object/
 */
function drealty_context_create_listingcontext($empty, $data = NULL, $conf = FALSE) {
  $context = new ctools_context('listingcontext');
  $context->plugin = 'listingcontext';

  if ($empty) {
    return $context;
  }

  if ($conf) {
    if (!empty($data)) {
      $listing = drealty_listing_load($data);
      $context->data = $listing;
      // For this simple item we'll just create our data by stripping non-alpha and
      // adding '_from_configuration_item_1' to it.

      $context->title = $listing->name;
      return $context;
    }
  } else {

    $listing = drealty_listing_load($data);
    $context->data = $listing;

    // $data is coming from an arg - it's just a string.
    // This is used for keyword.

    $context->title = $data;
    $context->argument = $data;

    $context->arg_length = strlen($context->argument);
    return $context;
  }
}

function listingcontext_settings_form($conf, $external = FALSE) {
  if (empty($conf)) {
    $conf = array(
        'mls_id' => 'mls id',
    );
  }
  $form = array();
  $form['mls_id'] = array(
      '#type' => 'textfield',
      '#title' => t('Setting for simplecontext'),
      '#size' => 50,
      '#description' => t('An example setting that could be used to configure a context'),
      '#default_value' => $conf['mls_id'],
      '#prefix' => '<div class="clear-block no-float">',
      '#suffix' => '</div>',
  );
  return $form;
}

/**
 * Provide a list of sub-keywords.
 *
 * This is used to provide keywords from the context for use in a content type,
 * pane, etc.
 */
function listingcontext_convert_list() {
  return array(
      'id' => t('id'),
      'listing_id' => t('Listing Id'),
      'listing_key' => t('Listing Key'),
      'latitude' => t('Latitude'),
      'longitude' => t('Longitude'),
      'list_office_mls_id' => t("Listing Office MLS Id"),
      'list_agent_mls_id' => t("Listing Agent MLS Id"),
  );
}

/**
 * Convert a context into a string to be used as a keyword by content types, etc.
 */
function listingcontext_convert($context, $type) {
  
  return $context->data->{$type};
//  switch ($type) {
//    case 'id':
//      return $context->data->id;
//    case 'listing_id':
//      return $context->data->listing_id;
//    case 'listing_key':
//      return $context->data->listing_key;
//  }
}

