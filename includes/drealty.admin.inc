<?php

function drealty_settings_form($form, &$form_state) {

  $form['general_settings'] = array('#type' => 'fieldset', '#title' => 'General Settings', '#collapsible' => TRUE, '#collapsed' => FALSE);


  $dc = new dRealtyConnection();
  $connections = $dc->FetchConnections();

  $dr = new drealtyResources();


  $form['general_settings']['connections'] = array();
  $form['general_settings']['connections']['#theme'] = 'drealty_general_setting_form';
  $form['general_settings']['connections']['#tree'] = TRUE;

  foreach ($connections as $key => $connection) {

    $form['general_settings']['connections'][$key]['name'] = array('#markup' => $connection->name);

    $resource_mappings = $dr->GetResourceMappings($connection->conid);

    //check to see if we actually have the office resource type setup, if not then this config option won't have any effect.
    if (isset($resource_mappings['drealty_office'])) {

      // next check to see if we have actually imported any offices
      $count = db_select('drealty_office', 'do')
        ->fields('do', array('id'))
        ->condition('conid', $connection->conid)
        ->execute()
        ->fetchAll();

      // check the count
      if (count($count) > 0) {

        // add the config field        
        $form['general_settings']['connections'][$key]['office_id'] = array(
            '#type' => 'textfield',
            '#default_value' => $connection->office_id,
            '#autocomplete_path' => "admin/drealty/office_autocomplete/$connection->conid",
            '#description' => t('Begin typing the name of the agency and select if from the autocmplete list. The office id will then be populated for you.'),
            '#title' => t('Office Id')
        );
      } else {
        $form['general_settings']['connections'][$key]['office_id'] = array(
            '#markup' => t("This connection does not appear to have any Office Resource Types imported. Please ensure you have it configured correctly !here and run the rets-import command from drush.", array("!here" => l('here', "admin/drealty/connections/manage/{$connection->name}/resource/drealty_office")))
        );
      }
    } else {
      $form['general_settings']['connections'][$key]['office_id'] = array(
          '#markup' => t("This connection does not appear to have the Office Resource Type configured. Configure it !here.", array("!here" => l('here', "admin/drealty/connections/manage/{$connection->name}/resource/drealty_office")))
      );
    }
  }


  $form["seo"] = array('#type' => 'fieldset', '#title' => "SEO - Search Engine Optimizations", '#collapsible' => TRUE, '#collapsed' => TRUE, '#tree' => TRUE);
  $types = array("drealty_listing" => "dRealty Listing", "drealty_openhouse" => "dRealty Open House", "drealty_agent" => "dRealty Agent", "drealty_office" => "dRealty Office");


  foreach ($types as $type => $name) {

    $form["seo"]["{$type}"] = array('#type' => 'fieldset', '#title' => "{$name}s", '#collapsible' => TRUE, '#collapsed' => TRUE);

    $default_title_pattern = strtoupper("{$type}_DEFAULT_TITLE_PATTERN");
    $default_url_pattern = strtoupper("{$type}_DEFAULT_URL_PATTERN");

    $form["seo"]["{$type}"]['page_title'] = array(
        '#type' => 'textfield',
        '#title' => t('Page Title Pattern'),
        '#maxlength' => 255,
        '#default_value' => ($title_pattern = variable_get("{$type}_title_pattern", FALSE)) ? $title_pattern : constant($default_title_pattern),
        '#size' => 150,
        '#description' => t('Pattern to use to create the page title.'),
    );

    $form["seo"]["{$type}"]['token_help'] = array(
        '#title' => t('Replacement patterns'),
        '#type' => 'fieldset',
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
    );
    $form["seo"]["{$type}"]['token_help']['token_tree'] = array(
        '#theme' => 'token_tree',
        '#token_types' => array($type),
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

  if (isset($form_state['values']['connections'])) {
    foreach ($form_state['values']['connections'] as $key => $value) {
      $connection = $dc->FetchConnection($key);
      $connection->office_id = $value['office_id'];
      $connection->save();
    }
  }

  foreach ($form_state['values']['seo'] as $key => $value) {
    variable_set("{$key}_title_pattern", $value['page_title']);
    variable_set("{$key}_url_pattern", $value['url_alias']);
  }
  cache_clear_all('drealty', 'cache', TRUE);
  drupal_set_message("The configuration options have been saved.");
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

function drealty_office_autocomplete_callback($conid, $string) {
  if ($conid == NULL) {
    exit();
  }

  $matches = array();

  $ored = db_or();
  $ored
    ->condition('office_name', '%' . drupal_strtolower($string) . '%', 'LIKE')
    ->condition('short_name', '%' . drupal_strtolower($string) . '%', 'LIKE');

  $anded = db_and();
  $anded
    ->condition('conid', $conid);

  $results = db_select('drealty_office', 'f')
    ->fields('f', array('office_id', 'office_name'))
    ->condition($anded)
    ->condition($ored)
    ->execute()
    ->fetchAll();

  foreach ($results as $data) {
    $matches[$data->office_id] = $data->office_name;
  }
  print drupal_json_encode($matches);
  exit();
}