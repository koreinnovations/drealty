<?php

/**
 * This field handler aggregates operations that can be done on a model
 * under a single field providing a more flexible way to present them in a view
 */
class drealty_listing_handler_mls_address_field extends views_handler_field {

  function construct() {
    parent::construct();

    $this->additional_fields['street_name'] = 'street_name';
    $this->additional_fields['street_number'] = 'street_number';
    $this->additional_fields['street_suffix'] = 'street_suffix';
    $this->additional_fields['city'] = 'city';
    $this->additional_fields['state_or_province'] = 'state_or_province';
    $this->additional_fields['postal_code'] = 'postal_code';
    $this->additional_fields['listing_id'] = 'listing_id';
  }

  function query() {
    $this->ensure_my_table();
    $this->add_additional_fields();
  }
public function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);
    $form['mls_images_link_to_property'] = array(
        '#type' => 'checkbox',
        '#default_value' => $this->options['mls_images_link_to_property'],
        '#description' => t('Link the address to the corresponding property page.'),
        '#title' => t('Link address to property.'),
    );
  }

  public function options_submit(&$form, &$form_state) {
    parent::options_submit($form, $form_state);
    $this->options['mls_images_link_to_property'] = $form_state['values']['mls_images_link_to_property'];
  }

  public function option_definition() {
    $options = parent::option_definition();
    $options['mls_images_link_to_property'] = TRUE;
    return $options;
  }
  function render($values) {

    $link = $this->options['mls_images_link_to_property'];
    $listing_id = $values->{$this->aliases['listing_id']};
    
    $street_number = isset($values->{$this->aliases['street_number']}) ? $values->{$this->aliases['street_number']} : '';
    $street_name = isset($values->{$this->aliases['street_name']}) ? $values->{$this->aliases['street_name']} : '';
    $street_suffix = isset($values->{$this->aliases['street_suffix']}) ? $values->{$this->aliases['street_suffix']} : '';

    $street = "{$street_number} {$street_name} {$street_suffix}";

    $address = "{$street} {$values->{$this->aliases['city']}}, {$values->{$this->aliases['state_or_province']}} {$values->{$this->aliases['postal_code']}}";
    
    $output .= ($link)?l($address, "listings/{$listing_id}", array('html' => TRUE)):$address;
    return $output;
  }

}
