<?php

class DrealtyLead extends Entity {
	
	public function __construct($values) {
		parent::__construct($values, 'drealty_lead');
	}
	
	protected function defaultLabel() {
    return $this->name;
  }

  public function label() {
    return $this->name;
  }
	
	/**
   * returns the full url() for this listing
   *
   *  @return string
   */
  public function url() {
    $uri = $this->uri();
    return url($uri['path'], $uri);
  }

  /**
   * Returns the drupal path to this listing
   *
   * @return string
   */
  public function path() {
    $uri = $this->uri();
    return $uri['path'];
  }

  public function defaultUri() {
    return array('path' => 'drealty_lead/' . $this->id);
  }
}

class DrealtyLeadController extends EntityAPIController {
	
	/**
   * @param type $entityType 
   */
  public function __construct($entityType) {
    parent::__construct($entityType);
  }

  /**
   *
   * @param type $entity
   * @param type $view_mode
   * @param type $langcode
   * @param type $content
   * @return string 
   */
  public function buildContent($entity, $view_mode = 'full', $langcode = NULL, $content = array()) {
    $content = parent::buildContent($entity, $view_mode, $langcode, $content);
    return $content;
  }

  public function save($entity, DatabaseTransaction $transaction = NULL) {
    parent::save($entity, $transaction);
  }
}
