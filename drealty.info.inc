<?php

function drealty_entity_property_info_alter(&$entity_info) {

  // add in the "extra" fields that we're storing in the schema for the entity admin ui

  $schema = drupal_get_schema_unprocessed("drealty", "drealty_listings");
  $schema_fields = $schema['fields'];
  
  foreach ($schema_fields as $key => $field) {
    $entity_info['drealty_listings']['properties'][$key]['schema field'] = $key;
    $entity_info['drealty_listings']['properties'][$key]['label'] = ucwords(str_replace("_", " ", $key));
    $entity_info['drealty_listings']['properties'][$key]['description'] = isset($field['description'])?$field['description'] : "";
    $entity_info['drealty_listings']['properties'][$key]['group'] = isset($field['group']) ? $field['group'] : NULL;
    $entity_info['drealty_listings']['properties'][$key]['weight'] = isset($field['weight']) ? $field['weight'] : NULL;
    $entity_info['drealty_listings']['properties'][$key]['required'] = isset($field['required']) ? $field['required'] : NULL;
    
    if(isset($field['multi']) && $field['multi']) {
      $entity_info['drealty_listings']['properties'][$key]['getter callback'] = 'drealty_search_api_multitext_getter_callback';
    }

    
  }
}