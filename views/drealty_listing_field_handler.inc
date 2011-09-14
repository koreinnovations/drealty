<?php

class drealty_listing_field_handler extends views_handler_field{
  
  function get_value($values, $field = NULL) {
    $alias = isset($field) ? $this->aliases[$field] : strtolower($this->field_alias);
    if (isset($values->{$alias})) {
      return $values->{$alias};
    }
  }
  
}
