<?php

function drealty_views_data_alter(&$data) {
  $data['drealty_connections']['edit_connection'] = array(
      'field' => array(
          'title' => t('Edit Connection Link'),
          'help' => t('Provide a link to the edit form for the connection'),
          'handler' => 'drealty_connection_handler_edit_link_field',
      ),
  );
  $data['drealty_connections']['operations'] = array(
      'field' => array(
          'title' => t('Operations links'),
          'help' => t('Display all operations available for this connection.'),
          'handler' => 'drealty_connection_handler_connection_operations_field',
      ),
  );
  $data['drealty_listing']['operations'] = array(
      'field' => array(
          'title' => t('Operations Links'),
          'help' => t('Display all operations available for this listing.'),
          'handler' => 'drealty_listing_handler_operations_field',
      ),
  );
  $data['drealty_listing']['mls_images'] = array(
      'field' => array(
          'title' => t('MLS Images'),
          'help' => t('Display images pulled from the MLS.'),
          'handler' => 'drealty_listing_handler_mls_images_field',
      ),
  );
  $data['drealty_listing']['mls_address'] = array(
            'field' => array(
          'title' => t('MLS Composite Address'),
          'help' => t('Display a formatted address. Street City, State Zip.'),
          'handler' => 'drealty_listing_handler_mls_address_field',
      ),
  );
  $data['drealty_openhouse']['listing_id'] = array(
      'relationship' => array(
          'handler' => 'views_handler_relationship',
          'base' => 'drealty_listing',
          'base field' => 'listing_id',
          'title' => t('Drealty Listing'),
          'label' => t('Get the Listing associated with an Open House.'),
      ),
  );
}

function drealty_views_default_views() {
  $views = array();

  $view = new view;
  $view->name = 'drealty_connections';
  $view->description = 'A list of all connections';
  $view->tag = 'drealty';
  $view->base_table = 'drealty_connections';
  $view->human_name = 'dRealty Connections';
  $view->core = 7;
  $view->api_version = '3.0-alpha1';
  $view->disabled = FALSE; /* Edit this to true to make a default view disabled initially */

  /* Display: Master */
  $handler = $view->new_display('default', 'Master', 'default');
  $handler->display->display_options['title'] = 'drealty connections';
  $handler->display->display_options['access']['type'] = 'none';
  $handler->display->display_options['cache']['type'] = 'none';
  $handler->display->display_options['query']['type'] = 'views_query';
  $handler->display->display_options['query']['options']['query_comment'] = FALSE;
  $handler->display->display_options['exposed_form']['type'] = 'basic';
  $handler->display->display_options['pager']['type'] = 'full';
  $handler->display->display_options['pager']['options']['items_per_page'] = '10';
  $handler->display->display_options['style_plugin'] = 'table';
  $handler->display->display_options['style_options']['columns'] = array(
      'conid' => 'conid',
      'name' => 'name',
  );
  $handler->display->display_options['style_options']['default'] = '-1';
  $handler->display->display_options['style_options']['info'] = array(
      'conid' => array(
          'sortable' => 0,
          'default_sort_order' => 'asc',
          'align' => '',
          'separator' => '',
      ),
      'name' => array(
          'sortable' => 0,
          'default_sort_order' => 'asc',
          'align' => '',
          'separator' => '',
      ),
  );
  $handler->display->display_options['style_options']['override'] = 1;
  $handler->display->display_options['style_options']['sticky'] = 0;
  $handler->display->display_options['style_options']['empty_table'] = 1;
  /* Field: DRealty Connection: Label */
  $handler->display->display_options['fields']['name']['id'] = 'name';
  $handler->display->display_options['fields']['name']['table'] = 'drealty_connections';
  $handler->display->display_options['fields']['name']['field'] = 'name';
  $handler->display->display_options['fields']['name']['alter']['alter_text'] = 0;
  $handler->display->display_options['fields']['name']['alter']['make_link'] = 0;
  $handler->display->display_options['fields']['name']['alter']['absolute'] = 0;
  $handler->display->display_options['fields']['name']['alter']['external'] = 0;
  $handler->display->display_options['fields']['name']['alter']['replace_spaces'] = 0;
  $handler->display->display_options['fields']['name']['alter']['trim_whitespace'] = 0;
  $handler->display->display_options['fields']['name']['alter']['nl2br'] = 0;
  $handler->display->display_options['fields']['name']['alter']['word_boundary'] = 1;
  $handler->display->display_options['fields']['name']['alter']['ellipsis'] = 1;
  $handler->display->display_options['fields']['name']['alter']['strip_tags'] = 0;
  $handler->display->display_options['fields']['name']['alter']['trim'] = 0;
  $handler->display->display_options['fields']['name']['alter']['html'] = 0;
  $handler->display->display_options['fields']['name']['element_label_colon'] = 1;
  $handler->display->display_options['fields']['name']['element_default_classes'] = 1;
  $handler->display->display_options['fields']['name']['hide_empty'] = 0;
  $handler->display->display_options['fields']['name']['empty_zero'] = 0;
  $handler->display->display_options['fields']['name']['hide_alter_empty'] = 0;
  /* Field: DRealty Connection: Version */
  $handler->display->display_options['fields']['version']['id'] = 'version';
  $handler->display->display_options['fields']['version']['table'] = 'drealty_connections';
  $handler->display->display_options['fields']['version']['field'] = 'version';
  $handler->display->display_options['fields']['version']['alter']['alter_text'] = 0;
  $handler->display->display_options['fields']['version']['alter']['make_link'] = 0;
  $handler->display->display_options['fields']['version']['alter']['absolute'] = 0;
  $handler->display->display_options['fields']['version']['alter']['external'] = 0;
  $handler->display->display_options['fields']['version']['alter']['replace_spaces'] = 0;
  $handler->display->display_options['fields']['version']['alter']['trim_whitespace'] = 0;
  $handler->display->display_options['fields']['version']['alter']['nl2br'] = 0;
  $handler->display->display_options['fields']['version']['alter']['word_boundary'] = 1;
  $handler->display->display_options['fields']['version']['alter']['ellipsis'] = 1;
  $handler->display->display_options['fields']['version']['alter']['strip_tags'] = 0;
  $handler->display->display_options['fields']['version']['alter']['trim'] = 0;
  $handler->display->display_options['fields']['version']['alter']['html'] = 0;
  $handler->display->display_options['fields']['version']['element_label_colon'] = 1;
  $handler->display->display_options['fields']['version']['element_default_classes'] = 1;
  $handler->display->display_options['fields']['version']['hide_empty'] = 0;
  $handler->display->display_options['fields']['version']['empty_zero'] = 0;
  $handler->display->display_options['fields']['version']['hide_alter_empty'] = 0;
  /* Field: DRealty Connection: Login_url */
  $handler->display->display_options['fields']['login_url']['id'] = 'login_url';
  $handler->display->display_options['fields']['login_url']['table'] = 'drealty_connections';
  $handler->display->display_options['fields']['login_url']['field'] = 'login_url';
  $handler->display->display_options['fields']['login_url']['label'] = 'Login URL';
  $handler->display->display_options['fields']['login_url']['alter']['alter_text'] = 0;
  $handler->display->display_options['fields']['login_url']['alter']['make_link'] = 0;
  $handler->display->display_options['fields']['login_url']['alter']['absolute'] = 0;
  $handler->display->display_options['fields']['login_url']['alter']['external'] = 0;
  $handler->display->display_options['fields']['login_url']['alter']['replace_spaces'] = 0;
  $handler->display->display_options['fields']['login_url']['alter']['trim_whitespace'] = 0;
  $handler->display->display_options['fields']['login_url']['alter']['nl2br'] = 0;
  $handler->display->display_options['fields']['login_url']['alter']['word_boundary'] = 1;
  $handler->display->display_options['fields']['login_url']['alter']['ellipsis'] = 1;
  $handler->display->display_options['fields']['login_url']['alter']['strip_tags'] = 0;
  $handler->display->display_options['fields']['login_url']['alter']['trim'] = 0;
  $handler->display->display_options['fields']['login_url']['alter']['html'] = 0;
  $handler->display->display_options['fields']['login_url']['element_label_colon'] = 1;
  $handler->display->display_options['fields']['login_url']['element_default_classes'] = 1;
  $handler->display->display_options['fields']['login_url']['hide_empty'] = 0;
  $handler->display->display_options['fields']['login_url']['empty_zero'] = 0;
  $handler->display->display_options['fields']['login_url']['hide_alter_empty'] = 0;
  /* Field: DRealty Connection: Active */
  $handler->display->display_options['fields']['active']['id'] = 'active';
  $handler->display->display_options['fields']['active']['table'] = 'drealty_connections';
  $handler->display->display_options['fields']['active']['field'] = 'active';
  $handler->display->display_options['fields']['active']['alter']['alter_text'] = 0;
  $handler->display->display_options['fields']['active']['alter']['make_link'] = 0;
  $handler->display->display_options['fields']['active']['alter']['absolute'] = 0;
  $handler->display->display_options['fields']['active']['alter']['external'] = 0;
  $handler->display->display_options['fields']['active']['alter']['replace_spaces'] = 0;
  $handler->display->display_options['fields']['active']['alter']['trim_whitespace'] = 0;
  $handler->display->display_options['fields']['active']['alter']['nl2br'] = 0;
  $handler->display->display_options['fields']['active']['alter']['word_boundary'] = 1;
  $handler->display->display_options['fields']['active']['alter']['ellipsis'] = 1;
  $handler->display->display_options['fields']['active']['alter']['strip_tags'] = 0;
  $handler->display->display_options['fields']['active']['alter']['trim'] = 0;
  $handler->display->display_options['fields']['active']['alter']['html'] = 0;
  $handler->display->display_options['fields']['active']['element_label_colon'] = 1;
  $handler->display->display_options['fields']['active']['element_default_classes'] = 1;
  $handler->display->display_options['fields']['active']['hide_empty'] = 0;
  $handler->display->display_options['fields']['active']['empty_zero'] = 0;
  $handler->display->display_options['fields']['active']['hide_alter_empty'] = 0;
  $handler->display->display_options['fields']['active']['format_plural'] = 0;
  /* Field: DRealty Connection: Operations links */
  $handler->display->display_options['fields']['operations']['id'] = 'operations';
  $handler->display->display_options['fields']['operations']['table'] = 'drealty_connections';
  $handler->display->display_options['fields']['operations']['field'] = 'operations';
  $handler->display->display_options['fields']['operations']['label'] = 'Operations';
  $handler->display->display_options['fields']['operations']['alter']['alter_text'] = 0;
  $handler->display->display_options['fields']['operations']['alter']['make_link'] = 0;
  $handler->display->display_options['fields']['operations']['alter']['absolute'] = 0;
  $handler->display->display_options['fields']['operations']['alter']['external'] = 0;
  $handler->display->display_options['fields']['operations']['alter']['replace_spaces'] = 0;
  $handler->display->display_options['fields']['operations']['alter']['trim_whitespace'] = 0;
  $handler->display->display_options['fields']['operations']['alter']['nl2br'] = 0;
  $handler->display->display_options['fields']['operations']['alter']['word_boundary'] = 1;
  $handler->display->display_options['fields']['operations']['alter']['ellipsis'] = 1;
  $handler->display->display_options['fields']['operations']['alter']['strip_tags'] = 0;
  $handler->display->display_options['fields']['operations']['alter']['trim'] = 0;
  $handler->display->display_options['fields']['operations']['alter']['html'] = 0;
  $handler->display->display_options['fields']['operations']['element_label_colon'] = 0;
  $handler->display->display_options['fields']['operations']['element_default_classes'] = 1;
  $handler->display->display_options['fields']['operations']['hide_empty'] = 0;
  $handler->display->display_options['fields']['operations']['empty_zero'] = 0;
  $handler->display->display_options['fields']['operations']['hide_alter_empty'] = 0;

  /* Display: Page */
  $handler = $view->new_display('page', 'Page', 'page');
  $handler->display->display_options['path'] = 'admin/drealty/connections/list';
  $handler->display->display_options['menu']['type'] = 'default tab';
  $handler->display->display_options['menu']['title'] = 'Connections';
  $handler->display->display_options['menu']['weight'] = '-10';
  $handler->display->display_options['menu']['name'] = 'management';
  $handler->display->display_options['tab_options']['type'] = 'normal';
  $handler->display->display_options['tab_options']['title'] = 'Connections';
  $handler->display->display_options['tab_options']['description'] = 'dRealty Connections';
  $handler->display->display_options['tab_options']['weight'] = '0';
  $handler->display->display_options['tab_options']['name'] = 'management';


  $views[$view->name] = $view;
  return $views;
}