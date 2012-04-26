<?php
/**
 * @file
 * dRealty listing entity classes.
 */

class DrealtyListing extends Entity {

  /**
   * The listing id.
   *
   * @var int
   */
  public $id;

  /**
   * The type of listing.
   *
   * @var string
   */
  public $type;

  /**
   * The connection id.
   *
   * @var int
   */
  public $conid;

  /**
   * MD5 Hash used to determine if a listing has changed.
   *
   * @var string
   */
  public $hash;

  /**
   *  Determines if images for this listing should be processed during import process.
   *
   * @var bool
   */
  public $process_images;

  /**
   * Determines if a listing was imported in a RETS feed.
   *
   * @var bool
   */
  public $rets_imported;

  /**
   * Status of the listing, 1 for active, 2 for inactive.
   *
   * @var int
   */
  public $status;

  /**
   * The listing key from the MLS RETS feed
   *
   * @var string
   */
  public $rets_key;
  public $rets_id;
  public $class;
  public $active;
  public $rets_photo_modification_timestamp;

  public function __construct(array $values = array()) {
    parent::__construct($values, 'drealty_listing');
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
    return array('path' => 'drealty_listing/' . $this->id);
  }

}

/**
 * 
 */
class DrealtyListingController extends EntityAPIController {

  /**
   *
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

class DrealtyListingType extends Entity {

  public $type;
  public $label;
  public $weight = 0;

  public function __construct(array $values = array()) {
    parent::__construct($values, 'drealty_listing_type');
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
    field_attach_delete_bundle('drealty_listing', $this->type);
    field_purge_batch(10);
  }

}

class DrealtyListingTypeController extends EntityAPIControllerExportable {

  public function delete($ids, DatabaseTransaction $transaction = NULL) {
    parent::delete($ids, $transaction);
    foreach ($ids as $id) {
      field_attach_delete_bundle('drealty_listing', $id);
      //check to see if any classes are using this bundle if so disable the class and set it's bundle to null
      db_update('drealty_classes')
        ->condition('bundle', $id)
        ->fields(array('bundle' => 'none', 'enabled' => 0))
        ->execute();
    }
  }

  /**
   *
   * @param DrealtyListingType $entity
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

  public function save($entity, DatabaseTransaction $transaction = NULL) {
    parent::save($entity, $transaction);

    if (isset($entity->fields)) {
      foreach ($entity->fields as $field) {
        if (!field_read_field($field['field_name'], array('include_inactive' => TRUE))) {
          field_create_field($field);
        } else {
          drupal_set_message(t('Skipped creating field %field. A field with this name already exists.', array('%field' => $field['field_name'])), 'warning');
        }
      }

      if (isset($entity->field_instances)) {
        foreach ($entity->field_instances as $instance) {
          $instance['bundle'] = $entity->type;
          field_create_instance($instance);
        }
      }
    }
  }

}