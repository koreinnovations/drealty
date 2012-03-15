<?php

class DrealtyAgent extends Entity {

  public function __construct(array $values = array()) {
    parent::__construct($values, 'drealty_agent');
  }

  protected function defaultLabel() {
    return "{$this->first_name} {$this->last_name}";
  }

  protected function defaultUri() {
    return array('path' => 'agents/' . "{$this->first_name}_{$this->last_name}");
  }

}

class DrealtyAgentController extends EntityAPIController {

  public function __construct($entityType) {
    parent::__construct($entityType);
  }

  public function create(array $values = array()) {
    $agent = parent::create($values);
    return $agent;
  }

}