<?php

/**
 * This field handler aggregates operations that can be done on a model
 * under a single field providing a more flexible way to present them in a view
 */
class drealty_connection_handler_connection_operations_field extends views_handler_field {

  function construct() {
    parent::construct();

    $this->additional_fields['conid'] = 'conid';
    $this->additional_fields['active'] = 'active';
    $this->additional_fields['name'] = 'name';
  }

  function query() {
    $this->ensure_my_table();
    $this->add_additional_fields();
  }

  function render($values) {

//    $links = menu_contextual_links('drealty', 'admin/drealty/connections', array($this->get_value($values, 'conid')));

    $links = array();

    $conid = $values->{$this->aliases['conid']};
    $name = $values->{$this->aliases['name']};
    $active = $values->{$this->aliases['active']};

    $operation = l("Edit", "admin/drealty/connections/manage/$name/edit") . ' | ' . l("Delete", "admin/drealty/connections/manage/$name/delete");
    if ($active) {
      $operation .= ' | ' . l("Deactivate", "admin/drealty/connections/manage/$name/deactivate");
      $operation .= ' | ' . l("Export", "admin/drealty/connections/manage/$name/export");
      $operation .= " | " . l("Developer Tools", "admin/drealty/connections/manage/$name/tools");
      $operation .= " | " . l("Data and Stats", "admin/drealty/connections/manage/$name/stats");

      // $operation .= ' | ' . l("Data Management", "admin/config/drealty/connection/manual_update/{$connection->conid}");
      $operation .= ' <br /> ' . "Configure: " . l("Properties", "admin/drealty/connections/manage/$name/resource/drealty_listing");
      $operation .= ' | ' . l("Open Houses", "admin/drealty/connections/manage/$name/resource/drealty_openhouse");
      $operation .= ' | ' . l("Offices", "admin/drealty/connections/manage/$name/resource/drealty_office");
      $operation .= ' | ' . l("Agents", "admin/drealty/connections/manage/$name/resource/drealty_agent");
    }
    else {
      $operation .= ' | ' . l("Activate", "admin/drealty/connections/manage/$name/activate");
    }

    return $operation;
  }

}
