<?php

function drealty_settings_form($form, &$form_state) {

  $form['general_settings'] = array('#type' => 'fieldset', '#title' => 'General Settings', '#collapsible' => TRUE, '#collapsed' => FALSE);
  $dr = new dRealtyConnection();
  $connections = $dr->FetchConnections();

  $form['general_settings']['connections'] = array();
  $form['general_settings']['connections']['#theme'] = 'drealty_general_setting_form';
  $form['general_settings']['connections']['#tree'] = TRUE;

  foreach ($connections as $key => $connection) {
    $form['general_settings']['connections'][$key]['name'] = array('#markup' => $connection->name);
    $form['general_settings']['connections'][$key]['office_id'] = array('#type' => 'textfield', '#default_value' => $connection->office_id, '#title' => t('Office Id'));
  }


  $form["seo"] = array('#type' => 'fieldset', '#title' => "SEO - Search Engine Optimizations", '#collapsible' => TRUE, '#collapsed' => TRUE);
  $types = array("drealty_listing" => "dRealty Listing", "drealty_openhouse" => "dRealty Open House", "drealty_agent" => "dRealty Agent", "drealty_office" => "dRealty Office");


  foreach ($types as $type => $name) {


    $form["seo"]["{$type}_title"] = array('#type' => 'fieldset', '#title' => "{$name}s", '#collapsible' => TRUE, '#collapsed' => TRUE);


    $default_title_pattern = strtoupper("{$type}_DEFAULT_TITLE_PATTERN");
    $default_url_pattern = strtoupper("{$type}_DEFAULT_URL_PATTERN");
    
    $form["seo"]["{$type}_title"]['page_title'] = array(
        '#type' => 'textfield',
        '#title' => t('Page Title Pattern'),
        '#default_value' => ($title_patttern = variable_get("drealty_listing_title_pattern", FALSE)) ? $title_patttern : constant($default_title_pattern),
        '#size' => 150,
        '#description' => t('Pattern to use to create the page title.'),
    );

    $form["seo"]["{$type}_title"]['url_alias'] = array(
        '#type' => 'textfield',
        '#title' => t('URL Pattern'),
        '#maxlength' => 255,
        '#default_value' => ($url_patttern = variable_get("drealty_listing_url_pattern", FALSE)) ? $url_pattern : constant($default_url_pattern),
        '#size' => 150,
        '#description' => t('Pattern to use to create the url alias'),
    );

    
    
    $form["seo"]["{$type}_title"]['token_help'] = array(
        '#title' => t('Replacement patterns'),
        '#type' => 'fieldset',
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
    );
    $form["seo"]["{$type}_title"]['token_help']['token_tree'] = array(
        '#theme' => 'token_tree',
        '#token_types' => array('drealty_listing'),
    );
  }


  $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save Settings'),
  );

  return $form;
}

function drealty_settings_form_submit($form, &$form_state) {

  $dc = new dRealtyConnection();

  foreach ($form_state['values']['connections'] as $key => $value) {
    $connection = $dc->FetchConnection($key);
    $connection->office_id = $value['office_id'];
    $connection->save();
  }
}

function theme_drealty_general_setting_form(&$variables) {
  $header = array('Connection Name', 'Office Id');
  $rows = array();
  $form = $variables['form'];
  foreach (element_children($form) as $key) {
    $row = array();
    $row[] = array('data' => drupal_render($form[$key]['name']));
    $row[] = array('data' => drupal_render($form[$key]['office_id']));
    $rows[] = $row;
  }
  $output = theme('table', array('header' => $header, 'rows' => $rows));

  return $output;
}

function blah() {
  return "Blah!";
}