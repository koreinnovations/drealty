<?php
// $Id$


function drealty_configure_system(&$form_state) {

  $form = array();

  $form['cron_settings'] = array('#type' => 'fieldset', '#title' => t('Cron Settings'),'#collapsible' => TRUE, '#weight' => -10);
  $form['cron_settings']['drealty_use_batch_api'] = array(
    '#type' => 'radios',
    '#title' => t('Import Method'),
    '#description' => t('Type of import method to use. The default is to use Drush which requires the setup of a cron job to initiate the import. Learn more !s', array('!s'=> l('here', '<front>'))),
    '#options' => array (1=>'Batch API', 0=>'Drush (default)'),
    '#default_value' => variable_get("drealty_use_batch_api", 1),
    '#required' => TRUE,
  );
  
  return system_settings_form($form);
}

