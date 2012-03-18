<?php

// dRealty Listing Entity Class
// ======================================================================================//
// ======================================================================================//
// ======================================================================================//


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
  public $listing_key;

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

}