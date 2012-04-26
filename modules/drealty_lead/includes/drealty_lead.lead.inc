<?php
/**
 * @file
 * dRealty lead entity.
 */

class DrealtyLead extends Entity {
	
	public $id;
	public $uid;
	public $created;
	public $changed;
	
	public function __construct($values = array()) {
		parent::__construct($values, 'drealty_lead');
	}
	
	protected function defaultLabel() {
    return $this->name;
  }

  public function label() {
    return $this->name;
  }
	
	protected function defaultUri() {
    return array('path' => 'drealty_lead/' . $this->id);
  }
	
	public function path() {
    $uri = $this->uri();
    return $uri['path'];
  }
	
	public function url() {
    $uri = $this->uri();
    return url($uri['path'], $uri);
  }
	
	public function save() {
		if (empty($this->created) && (!empty($this->is_new) || !$this->id)) {
      $this->created = REQUEST_TIME;
    }
		
		if (empty($this->uid)) {
			global $user;
			$this->uid = $user->uid;
		}
		
    $this->changed = REQUEST_TIME;

    parent::save();
  }
	
	public function delete() {
    parent::delete();
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
	
	public function create(array $values = array()) {
    $entity = parent::create($values);
    return $entity;
  }

  public function save($entity, DatabaseTransaction $transaction = NULL) {
    parent::save($entity, $transaction);
  }
	
	public function delete($ids, DatabaseTransaction $transaction = NULL) {

    $entities = $ids ? $this->load($ids) : FALSE;
    
		if (!$entities) {
      // Do nothing, in case invalid or no ids have been passed.
      return;
    }

    parent::delete($ids, $transaction);
  }
}
