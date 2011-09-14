<?php

class drealty_connection_handler_edit_link_field extends views_handler_field {

  function render($values) {
    $text = !empty($this->options['text']) ? $this->options['text'] : t('edit');
    $conid = $values->{$this->aliases['conid']};
    return l($text, 'admin/drealty/connections/manage/' . $conid . '/edit');
  }

}
