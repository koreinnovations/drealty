<?php

// dRealty Listing Entity Class
// ======================================================================================//
// ======================================================================================//
// ======================================================================================//


class DrealtyListing extends Entity {

  public function __construct(array $values = array()) {
    parent::__construct($values, 'drealty_listing');
  }

  protected function defaultLabel() {
    return $this->name;
  }

  protected function defaultUri() {
    return array('path' => 'listings/' . $this->listing_id);
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
    
    $content['#theme'] = 'entity';
    $content['#element'] = $entity;
    $content['#view_mode'] = $view_mode;
    
//
//  
    foreach ($entity as $key => $value) {
      $content[$key] = array(
          '#value' => $value,
          '#markup' => empty($value) ? NULL : "<div class='field-value'>$value</div>",
          '#prefix' => empty($value) ? NULL : "<div>\n<div class='field-label'>$key</div>",
          '#suffix' => empty($value) ? NULL : "</div>",
      );
    }
//
//
//
    $fids = db_select('file_usage', 'f')
            ->fields('f', array('fid'))
            ->condition('id', $entity->id)
            ->condition('type', 'drealty_listing')
            ->execute()
            ->fetchAllAssoc('fid');

    if ($fids) {
      $fids = array_keys($fids);
      $images = file_load_multiple($fids);

      $entity->mls_images = array();

      $content['mls_images'] = array();
      $content['mls_images']['#prefix'] = "<div class='field-label'>MLS Images</div>";

      foreach ($images as $key => $image) {

        $entity->mls_images[] = $image;

        $uri = array(
            'path' => file_create_url($image->uri),
            'options' => array(),
        );

        $image->title = $entity->listing_id;
        $image->alt = $entity->listing_id;
        $image->height = '';
        $image->width = '';

        $content['mls_images'][$key] = array(
            '#theme' => 'image_formatter',
            '#item' => (array) $image,
            '#path' => '',
            '#width' => '',
            '#height' => '',
        );
      }
    }

    $entity->drealty_path_alias = path_load("listings/$entity->listing_id");

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
//
//    $token_string = ($pattern = variable_get("drealty_listing_title_pattern", FALSE)) ? $pattern : DREALTY_LISTING_DEFAULT_TITLE_PATTERN;
//
//    if (extension_loaded("mbstring")) {
//      $title = mb_convert_case(token_replace($token_string, array('drealty_listing' => $entity)), MB_CASE_TITLE);
//    } else {
//      $title = ucwords(drupal_strtolower(token_replace($token_string, array('drealty_listing' => $entity))));
//    }
//
//
//    drupal_set_title($title);
//
//    $og_title = array(
//        '#tag' => 'meta',
//        '#attributes' => array(
//            'property' => 'og:title',
//            'content' => $title,
//        ),
//    );

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
//    drupal_add_html_head($og_title, 'og_title');


    return $content;
  }

  public function attachLoad(&$queried_entities, $revision_id = FALSE) {
    parent::attachLoad($queried_entities, $revision_id);
// populate a path alias
  }

  public function create(array $values = array()) {
    $listing = parent::create($values);
    return $listing;
  }

  public function delete($ids, DatabaseTransaction $transaction = NULL) {

    $entities = $ids ? $this->load($ids) : FALSE;
    if (!$entities) {
      // Do nothing, in case invalid or no ids have been passed.
      return;
    }
    foreach ($entities as $entity) {
      if (isset($entity->mls_images)) {
        foreach ($entity->mls_images as $file) {
          file_delete($file, TRUE);
        }
      }
      if (isset($entity->drealty_path_alias)) {
        path_delete($entity->drealty_path_alias['pid']);
      }
    }


    parent::delete($ids, $transaction);
  }

  public function save($entity, DatabaseTransaction $transaction = NULL, $reset = FALSE) {
    parent::save($entity, $transaction);

    if ((isset($entity->is_new) && $entity->is_new) || $reset) {
//      if (isset($entity->latitude) && isset($entity->longitude)) {
//        db_query("UPDATE {drealty_listing} SET geo = GeomFromText('POINT({$entity->latitude} {$entity->longitude})') WHERE id = {$entity->id}", array());
//      }

      $path = array();
      if (isset($entity->drealty_path_alias)) {
        $path['pid'] = $entity->drealty_path_alias['pid'];
      }

      $token_string = ($pattern = variable_get("drealty_listing_url_pattern", FALSE)) ? $pattern : DREALTY_LISTING_DEFAULT_URL_PATTERN;

      $alias = token_replace($token_string, array('drealty_listing' => $entity));

      $path['source'] = "listings/$entity->listing_id";
      $path['alias'] = drupal_strtolower(str_replace(" ", "_", $alias));


      path_save($path);
    }
  }

}