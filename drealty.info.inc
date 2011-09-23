<?php

function drealty_entity_property_info_alter(&$entity_info) {

  // add in the "extra" fields that we're storing in the schema for the entity admin ui
  $entities = array('drealty_listing', 'drealty_openhouse', 'drealty_agent', 'drealty_office');
  foreach ($entities as $entity) {
    $schema = drupal_get_schema_unprocessed("drealty", $entity);
    $schema_fields = $schema['fields'];

    foreach ($schema_fields as $key => $field) {
      $entity_info[$entity]['properties'][$key]['schema field'] = $key;
      $entity_info[$entity]['properties'][$key]['label'] = ucwords(str_replace("_", " ", $key));
      $entity_info[$entity]['properties'][$key]['description'] = isset($field['description']) ? $field['description'] : "";
      $entity_info[$entity]['properties'][$key]['group'] = isset($field['group']) ? $field['group'] : NULL;
      $entity_info[$entity]['properties'][$key]['weight'] = isset($field['weight']) ? $field['weight'] : NULL;
      $entity_info[$entity]['properties'][$key]['required'] = isset($field['required']) ? $field['required'] : NULL;

      if (isset($field['multi']) && $field['multi']) {
        $entity_info[$entity]['properties'][$key]['type'] = 'list<text>';
        $entity_info[$entity]['properties'][$key]['getter callback'] = 'drealty_search_api_multitext_getter_callback';
      }
      if (isset($field['entity_property_type']) && $field['entity_property_type']) {
        $entity_info[$entity]['properties'][$key]['type'] = $field['entity_property_type'];
      }
    }
  }
}