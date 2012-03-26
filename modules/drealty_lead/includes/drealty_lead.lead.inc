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

class DrealtyLeadType extends Entity {

	public $type;
  public $label;
  public $weight = 0;

  public function __construct(array $values = array()) {
    parent::__construct($values, 'drealty_lead_type');
  }

  /**
   * Returns whether the profile type is locked, thus may not be deleted or renamed.
   *
   * Profile types provided in code are automatically treated as locked, as well
   * as any fixed profile type.
   */
  public function isLocked() {
    return isset($this->status) && empty($this->is_new) && (($this->status & ENTITY_IN_CODE) || ($this->status & ENTITY_FIXED));
  }
  
  public function delete() {
    parent::delete();
    field_attach_delete_bundle('drealty_lead', $this->type);
    field_purge_batch(10);
  }
}

class DrealtyLeadTypeController extends EntityAPIControllerExportable {

	/**
	 *
	 * @param type $ids
	 * @param DatabaseTransaction $transaction 
	 */
	
	public function delete($ids, DatabaseTransaction $transaction = NULL) {
    parent::delete($ids, $transaction);
    foreach ($ids as $id) {
      field_attach_delete_bundle('drealty_lead', $id);
    }
  }
	
 /**
  * @param DrealtyLeadType $entity
  * @param string $prefix
  * @return string 
  */
	
  public function export($entity, $prefix = '') {
    $vars = get_object_vars($entity);
    unset($vars[$this->statusKey], $vars[$this->moduleKey], $vars['is_new'], $vars['type'], $vars['label']);
    if ($this->nameKey != $this->idKey) {
      unset($vars[$this->idKey]);
    }
    $info = $entity->entityInfo();
    $instances = field_info_instances($info['bundle of'], $entity->type);

    $fields = array();
    foreach ($instances as &$instance) {
      $field = field_info_field($instance['field_name']);
      unset($field['id'], $field['bundles'], $field['storage']);
      $fields[$instance['field_name']] = $field;
      unset($instance['bundle']);
    }

    $vars['fields'] = $fields;
    $vars['field_instances'] = $instances;

    return entity_var_json_export($vars, $prefix);
  }
  
	/**
	 *
	 * @param type $entity
	 * @param DatabaseTransaction $transaction 
	 */
	
	public function save($entity, DatabaseTransaction $transaction = NULL) {
    parent::save($entity, $transaction);
  }

}