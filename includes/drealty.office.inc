<?php

class DrealtyOffice extends Entity {

  public function __construct(array $values = array()) {
    parent::__construct($values, 'drealty_office');
  }

  protected function defaultLabel() {
    return $this->id;
  }

  protected function defaultUri() {
    return array('path' => 'offices/' . $this->office_id);
  }

}

class DrealtyOfficeController extends EntityAPIController {

  public function __construct($entityType) {
    parent::__construct($entityType);
  }

  public function create(array $values = array()) {
    $office = parent::create($values);
    return $office;
  }

}