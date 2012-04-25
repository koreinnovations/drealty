<?php

/**
 *
 * Implements hook_entity_property_info
 * 
 */
function drealty_entity_property_info() {
  $info = array();
  $properties = &$info['drealty_listing']['properties'];

  $properties['id'] = array(
    'label' => t('Drealty Listing Id'),
    'type' => 'integer',
    'description' => t('The unique ID of the drealty listing'),
    'schema field' => 'id',
  );
  $properties['type'] = array(
    'label' => t('Drealty Listing Type'),
    'type' => 'text',
    'description' => t('The drealty property type'),
    'schema field' => 'type',
  );
  $properties['rets_key'] = array(
    'label' => t('Drealty Listing RETS Key'),
    'type' => 'text',
    'description' => t('The RETS System Unique ID for the drealty listing'),
    'schema field' => 'rets_key',
  );
  $properties['rets_id'] = array(
    'label' => t('Drealty Listing RETS ID'),
    'type' => 'text',
    'description' => t('The RETS Public Unique ID for the drealty listing'),
    'schema field' => 'rets_id',
  );
  $properties['label'] = array(
    'label' => t('Drealty Listing Label'),
    'type' => 'text',
    'description' => t('The RETS Public Unique ID for the drealty listing'),
    'schema field' => 'rets_id',
  );
  return $info;
}