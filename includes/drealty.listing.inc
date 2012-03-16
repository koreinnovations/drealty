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

    $entity->drealty_path_alias = path_load("drealty_listing/$entity->id");

//    $og_description = array(
//        '#tag' => 'meta',
//        '#attributes' => array(
//            'property' => 'og:description',
//            'content' => $entity->public_remarks,
//        ),
//    );
//
//    $meta_description = array(
//        '#tag' => 'meta',
//        '#attributes' => array(
//            'name' => 'description',
//            'content' => $entity->public_remarks,
//        ),
//    );

    $token_string = ($pattern = variable_get("drealty_listing_title_pattern", FALSE)) ? $pattern : DREALTY_LISTING_DEFAULT_TITLE_PATTERN;

    if (extension_loaded("mbstring")) {
      $title = mb_convert_case(token_replace($token_string, array('drealty_listing' => $entity)), MB_CASE_TITLE);
    } else {
      $title = ucwords(drupal_strtolower(token_replace($token_string, array('drealty_listing' => $entity))));
    }


    drupal_set_title($title);

    $og_title = array(
        '#tag' => 'meta',
        '#attributes' => array(
            'property' => 'og:title',
            'content' => $title,
        ),
    );

//    if (isset($entity->mls_images)) {
//      $og_image = array(
//          '#tag' => 'meta',
//          '#attributes' => array(
//              'property' => 'og:image',
//              'content' => file_create_url(array_shift($entity->mls_images)->uri),
//          ),
//      );
//      drupal_add_html_head($og_image, 'og_image');
//    }

//    drupal_add_html_head($meta_description, 'meta_description');
//    drupal_add_html_head($og_description, 'og_description');
    drupal_add_html_head($og_title, 'og_title');


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