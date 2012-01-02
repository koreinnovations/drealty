<?php

/**
 * This field handler aggregates operations that can be done on a model
 * under a single field providing a more flexible way to present them in a view
 */
class drealty_listing_handler_mls_images_field extends views_handler_field {

  function construct() {
    parent::construct();

    $this->additional_fields['property_id'] = array('table' => 'drealty_listing', 'field' => 'id');
    $this->additional_fields['property_listing_id'] = array('table' => 'drealty_listing', 'field' => 'listing_id');
  }

  function query() {
    $this->ensure_my_table();
    $this->add_additional_fields();
  }

  public function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);

    $styles = image_styles();

    $options = array();
    $options['original'] = 'original';
    foreach ($styles as $key => $style) {
      $options[$key] = $key;
    }

    $form['mls_image_style'] = array(
        '#type' => 'select',
        '#options' => $options,
        '#default_value' => $this->options['mls_image_style'],
        '#description' => t('Apply one of the Image Styles to this image.'),
        '#title' => t('Image Style'),
    );
    $form['mls_images_to_display'] = array(
        '#type' => 'textfield',
        '#default_value' => $this->options['mls_images_to_display'],
        '#description' => t('The number of images to display, 0 for unlimited'),
        '#title' => t('Number of Images'),
    );
    $form['mls_images_link_to_property'] = array(
        '#type' => 'checkbox',
        '#default_value' => $this->options['mls_images_link_to_property'],
        '#description' => t('Link the image(s) to their corresponding property.'),
        '#title' => t('Link image to property.'),
    );
  }

  public function options_submit(&$form, &$form_state) {
    parent::options_submit($form, $form_state);

    $this->options['mls_image_style'] = $form_state['values']['mls_image_style'];
    $this->options['mls_images_to_display'] = $form_state['values']['mls_images_to_display'];
    $this->options['mls_images_link_to_property'] = $form_state['values']['mls_images_link_to_property'];
  }

  public function option_definition() {
    $options = parent::option_definition();
    $options['mls_image_style'] = array(
        'default' => 'original',
        'translatable' => FALSE,
    );
    $options['mls_images_to_display'] = array(
        'default' => 1,
        'translatable' => FALSE,
    );
    $options['mls_images_link_to_property'] = array(
        'default' => TRUE,
        'translatable' => FALSE,
    );
    return $options;
  }

  function render($values) {

    $links = array();
    $output = '';

    $id = $values->{$this->aliases['property_id']};

    $fids = db_select('file_usage', 'f')
      ->fields('f', array('fid'))
      ->condition('id', $id)
      ->execute()
      ->fetchAllAssoc('fid');
    if ($fids) {
      $fids = array_keys($fids);
      $files = file_load_multiple($fids);
    }

    $images = array_values($files);

    $style_name = $this->options['mls_image_style'];
    $count = $this->options['mls_images_to_display'];
    $link = $this->options['mls_images_link_to_property'];
    $listing_id = $values->{$this->aliases['property_listing_id']};

    $limit = ($count > count($files)) ? count($files) : $count;

    for ($i = 0; $i < $limit; $i++) {
      if ($style_name == 'original') {
        $img = theme_image(array('path' => $images[$i]->uri, 'alt' => '', 'width' => '', 'height' => ''));
      } else {
        $img = theme_image_style(array('style_name' => $style_name, 'path' => $images[$i]->uri, 'alt' => '', 'width' => '', 'height' => ''));
      }

      $output .= ($link) ? l($img, "listings/{$listing_id}", array('html' => TRUE)) : $img;
    }



    return $output;
  }

}
