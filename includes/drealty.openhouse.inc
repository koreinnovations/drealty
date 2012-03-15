<?php

class DrealtyOpenHouse extends Entity {

  public function __construct(array $values = array()) {
    parent::__construct($values, 'drealty_openhouse');
  }

  protected function defaultLabel() {
    return $this->id;
  }

  protected function defaultUri() {
    return array('path' => 'openhouses/' . $this->listing_id);
  }

}

class DrealtyOpenHouseController extends EntityAPIController {

  public function __construct($entityType) {
    parent::__construct($entityType);
  }

  public function attachLoad(&$queried_entities, $revision_id = FALSE) {
    parent::attachLoad($queried_entities, $revision_id);

    foreach ($queried_entities as $entity) {
      $query = new EntityFieldQuery();
      $result = $query
        ->entityCondition('entity_type', 'drealty_listing')
        ->propertyCondition('listing_key', $entity->listing_key)
        ->execute();
      if ($result) {
        $entity->listing = reset(entity_load('drealty_listing', array_keys($result['drealty_listing']), array(), FALSE));
      }
    }
  }

  public function create(array $values = array()) {
    $openhouse = parent::create($values);
    return $openhouse;
  }

}