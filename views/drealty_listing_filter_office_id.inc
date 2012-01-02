<?php

class drealty_listing_filter_office_id extends views_handler_filter {

  function query() {

    $dc = new dRealtyConnection();
    $connections = $dc->FetchConnections();

    $db_or = db_or();
    foreach ($connections as $connection) {
      if (isset($connection->office_id)) {
        $db_or->where("list_office_mls_id = '{$connection->office_id}'");
      }
    }
    $this->query->add_where(0, $db_or);
  }

}
