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