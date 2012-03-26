<?php

function drealty_connection_entity_save(drealtyConnectionEntity $connection) {
  return $connection->save();
}

class drealtyConnection {

  /**
   *
   * @var phRETS 
   */
  public $rets = NULL;

  /**
   *
   * @var string 
   */
  private $error_string = '';

  /**
   *
   * @var bool 
   */
  private $connected = FALSE;

  /**
   *
   * @var drealtyConnectionEntity 
   */
  private $current_connection = NULL;

  public function __construct() {
    drealty_load_phrets();
    $this->rets = new phRETS();
  }

  public function __destruct() {
    // If we're still connected, someone forgot to disconnect.
    if ($this->rets && $this->connected) {
      $backtrace = debug_backtrace();
      watchdog('drealty', 'Connection to server still active, someone forgot it open in @func(). Attempting to disconnect.', array('@func' => $backtrace[1]['function']));
      $this->disconnect();
    }
  }

  /**
   *
   * @param int $conid
   * @return bool
   */
  public function connect($conid) {

    // check to see if we  are connected to a rets server. If there is a connection
    // do nothing.

    if (!$this->connected) {
      // need to setup a connection
      if ($this->current_connection) {
        try {
          return $this->handle_connection();
        } catch (drealty_rets_connection_exception $e) {
          watchdog('drealty', $e->getMessage(), NULL, WATCHDOG_ERROR);
          return FALSE;
        }
      } else {

        $this->current_connection = $this->FetchConnection($conid);

        $error = FALSE;
        // force basic authentication
        if ($this->current_connection->force_basic_auth) {
          $this->rets->SetParam('force_basic_authentication', TRUE);
        }
        // enable compression
        if ($this->current_connection->use_compression) {
          $this->rets->SetParam('compression_enabled', TRUE);
        }

        if ($this->current_connection->use_interealty_auth) {
          $this->rets->SetParam('use_interealty_ua_auth', TRUE);
        }

        if ($this->current_connection->debug_mode) {
          $this->rets->SetParam('debug_mode', true);
          $path = variable_get('file_private_path', FALSE);
          if ($path && !empty($path)) {
            $this->rets->SetParam("debug_file", "private://drealty_debug_log.txt");
          } else {
            $this->rets->SetParam("debug_file", "public://drealty_debug_log.txt");
          }
        }
        // add some headers
        $this->rets->AddHeader("User-Agent", $this->current_connection->ua_string);
        $this->rets->AddHeader("RETS-Version", "RETS/" . $this->current_connection->version);

        try {
          return $this->handle_connection();
        } catch (drealty_rets_connection_exception $e) {
          watchdog('drealty', $e->getMessage(), NULL, WATCHDOG_ERROR);
          return FALSE;
        }
      }
    }
  }

  /**
   * Function to perform the actual connection
   * @return bool
   */
  protected function handle_connection() {

    $current_connection = $this->current_connection;
    if (!$this->rets->Connect($this->current_connection->login_url, $this->current_connection->username, $this->current_connection->password, $this->current_connection->ua_password)) {
      // we didn't connect, check to see if we have any error results
      if ($this->rets->Error()) {
        // error result present, use that in our exception
        $error_info = $this->rets->Error();
        $this->error_string = $error_info['text'];
        throw new drealty_rets_connection_exception($error_info['text'], $error_info['code']);
      } else {
        // no error result present so we'll throw a generic exception
        throw new drealty_rets_connection_exception('There was an error connecting to the RETS Server.');
      }
    } else {
      // connection was successful!
      $this->connected = TRUE;
      return TRUE;
    }
  }

  /**
   * Disconnect from the RETS server
   *
   * @return bool
   */
  public function disconnect() {
    // disconnect if we are connected.

    if ($this->rets && $this->connected) {
      if ($this->rets->Disconnect()) {
        $this->connected = FALSE;
        if (function_exists('drush_log')) {
          drush_log("Disconnected from RETS server.");
        }
        return TRUE;
      } else {
        $error = $this->rets->Error();
        throw new drealty_rets_disconnect_exception("Error Disconnecting from the RETS Server: (Type: {$error['type']} Code: {$error['code']} Msg: {$error['text']})");
      }
    }
  }

  /**
   *
   * @return drealtyConnectionEntity[] 
   */
  public function FetchConnections() {
    return entity_load('drealty_connection_entity');
  }

  /**
   *
   * @param int $id
   * @return drealtyConnectionEntity 
   */
  public function FetchConnection($id) {
    $connection = entity_load('drealty_connection_entity', array($id));
    return reset($connection);
  }

}

class drealtyConnectionEntity extends Entity {

  public $conid = 0;
  public $name;
  public $label;
  public $login_url;
  public $username;
  public $password;
  public $ua_string;
  public $ua_password;
  public $version;
  public $use_interealty_auth;
  public $force_basic_auth;
  public $use_compression;
  public $active;
  public $resources;
  public $resource_mappings;
  public $classes;
  public $fields;
  public $field_mappings;
  public $debug_mode = FALSE;

  public function __construct($values = array()) {
    parent::__construct($values, 'drealty_connection_entity');
  }

  protected function defaultLabel() {
    return $this->label;
  }

  public function label() {
    return $this->label;
  }

  /**
   *
   * @return drealtyRetsResource[] 
   */
  public function Resources() {
    $dm = new drealtyMetaData();
    return $dm->FetchResources($this->conid);
  }

  /**
   *
   * @return drealtyResourceMapping[] 
   */
  public function ResourceMappings() {
    $dm = new drealtyMetaData();
    return $dm->GetResourceMappings($this->conid);
  }

  /**
   *
   * @param drealtyRetsResource $resource
   * @return drealtyRetsClass[] 
   */
  public function FetchClasses($resource = NULL) {
    $dm = new drealtyMetaData();
    return $dm->FetchClasses($this->conid, $resource);
  }

  /**
   *
   * @return drealtyRetsClass[] 
   */
  public function FetchEnabledClasses() {
    $dm = new drealtyMetaData();
    return $dm->FetchEnabledClasses($this->conid);
  }

  /**
   *
   * @param drealtyRetsResource $resource
   * @return drealtyRetsField[] 
   */
  public function FetchFields($resource, $class) {
    $dm = new drealtyMetaData();
    return $dm->FetchFields($this->conid, $resource, $class);
  }

  /**
   *
   * @param int $conid
   * @param drealtyRetsResource $resource
   * @param drealttyRetsClass $class
   * @return drealtyFieldMapping[]
   */
  public function FetchFieldMappings($resource = NULL, $class = NULL) {
    $dm = new drealtyMetaData();
    return $dm->FetchFieldMappings($this->conid, $resource, $class);
  }

  /**
   *
   * @param string $resource
   * @param string $field_name
   * @param string $systemname
   * @param int $class
   * @param string $field_api_type
   * @param array $data 
   */
  public function CreateFieldMapMapping($rid, $field_name, $systemname, $class_id, $field_api_type = 'drealty', $data = NULL, $hash_exclude = 0) {
    // remove any mapping and create a new one
    $this->RemoveFieldMapping($rid, $field_name, $class_id);

    db_insert('drealty_field_mappings')
      ->fields(array(
        'conid' => $this->conid,
        'rid' => $rid,
        'field_name' => $field_name,
        'systemname' => $systemname,
        'field_api_type' => $field_api_type,
        'data' => isset($data) ? serialize($data) : NULL,
        'cid' => $class_id,
        'hash_exclude' => $hash_exclude,
      ))
      ->execute();
  }

  public function RemoveFieldMapping($rid, $field_name, $class) {
    db_delete("drealty_field_mappings")
      ->condition("conid", $this->conid)
      ->condition("rid", $rid)
      ->condition("field_name", $field_name)
      ->condition('cid', $class)
      ->execute();
  }

  public function delete() {
    $dm = new drealtyMetaData();

    $dm->RemoveFields($this->conid);
    $dm->RemoveFieldMappings($this->conid);
    $dm->RemoveClasses($this->conid);
    $dm->RemoveResourceMappings($this->conid);
    $dm->RemoveResources($this->conid);
    parent::delete();
  }

}

class drealtyConnectionEntityContoller extends EntityAPIControllerExportable {

  public function __construct($entityType) {
    parent::__construct($entityType);
  }

  public function create(array $values = array()) {
    // Add values that are specific to our Model
    $values += array(
      'conid' => NULL,
      'name' => NULL,
      'login_url' => NULL,
      'username' => NULL,
      'password' => NULL,
      'ua_string' => 'DREALTY/1.0',
      'ua_password' => '',
      'version' => '1.5',
      'use_interealty_auth' => '0',
      'force_basic_auth' => '0',
      'use_compression' => '0',
      'active' => FALSE,
      'debug_mode' => FALSE,
    );
    $dmealtyConnectionEntity = parent::create($values);
    return $dmealtyConnectionEntity;
  }

  /**
   *
   * @param drealtyConnectionEntity $entity
   * @param string $prefix 
   */
  public function export($entity, $prefix = '') {
    $entity->active = 0;


    $dm = new drealtyMetaData();
    $resources = $dm->FetchResources($entity->conid);
    $bundles = array();

    foreach ($resources as &$resource) {

      $resource->classes = $dm->FetchClasses($entity->conid, $resource);

      foreach ($resource->classes as &$class) {

        $class->resource = $resource->systemname;
        $class->fields = $dm->FetchFields($entity->conid, $resource, $class);
        if (!empty($class->bundle) && $class->bundle != 'none') {
          $bundles[] = $class->bundle;
        }

        foreach ($class->fields as &$field) {
          $field->resource = $resource->systemname;
          unset($field->fid, $field->conid, $field->rid);
        }

        unset($class->cid, $class->conid, $class->rid);
      }
      unset($resource->rid, $resource->conid);
    }

    $resource_mappings = $dm->GetResourceMappings($entity->conid);

    foreach ($resource_mappings as &$resource_mapping) {
      $resource_mapping->resource_name = $dm->FetchResource($resource_mapping->rid)->systemname;
      unset($resource_mapping->mid, $resource_mapping->conid, $resource_mapping->rid);
    }

    $entity->field_instances = array();
    $entity->field_fields = array();
    foreach ($bundles as $bundle) {

      $field_instances = field_info_instances('drealty_listing', $bundle);
      $entity->field_instances[$bundle] = $field_instances;

      foreach ($field_instances as &$instance) {
        $field = field_info_field($instance['field_name']);
        unset($field['id'], $field['bundles'], $field['storage']);
        $entity->field_fields[$instance['field_name']] = $field;
      }
    }

    $field_mappings = $dm->ExportFieldMappings($entity->conid);
    foreach ($field_mappings as &$field_mapping) {
      $field_mapping->resource_name = $dm->FetchResource($field_mapping->rid)->systemname;
      $field_mapping->class_name = $dm->FetchClass($field_mapping->cid)->systemname;
      unset($field_mapping->conid, $field_mapping->rid, $field_mapping->cid);
    }

    $entity->field_mappings = $field_mappings;
    $entity->resource_mappings = $resource_mappings;
    $entity->resources = $resources;

    //unset($entity->conid);
    $vars = get_object_vars($entity);
    unset($vars['conid']);
    return entity_var_json_export($vars, $prefix);
  }

  public function import($export) {
    if (($entity = parent::import($export)) !== FALSE) {
      $entity->imported = true;
      return $entity;
    }
    return FALSE;
  }

  public function save($entity, DatabaseTransaction $transaction = NULL) {
    parent::save($entity, $transaction);

    if (isset($entity->imported) && $entity->imported) {
      $bundles = array();
      $resources = array();
      $classes = array();


      foreach ($entity->resources as &$resource) {

        $resource['conid'] = $entity->conid;
        drupal_write_record('drealty_resources', $resource);
        $resources[$resource['systemname']] = $resource;

        foreach ($resource['classes'] as &$class) {

          $class['rid'] = $resource['rid'];
          $class['conid'] = $entity->conid;
          drupal_write_record('drealty_classes', $class);

          $classes[$class['systemname']] = $class;
          if (!empty($class['bundle'])) {
            $bundles[$class['bundle']] = $class['bundle'];
          }

          foreach ($class['fields'] as &$field) {
            $field['conid'] = $entity->conid;
            $field['rid'] = $resource['rid'];
            drupal_write_record('drealty_fields', $field);
          }
        }
      }

      foreach ($bundles as $bundle) {

        // check to see if there is already a drealty_listing_type with this name
        if (!drealty_listing_type_name_exists($bundle)) {
          $type = entity_create('drealty_listing_type', array(
            'type' => $bundle,
            'label' => t($bundle),
            'weight' => 0,
            'locked' => 0,
            ));
          $type->save();
        }
      }

      foreach ($entity->resource_mappings as $resource_mapping) {
        $resource_mapping['conid'] = $entity->conid;
        $resource_mapping['rid'] = $resources[$resource_mapping['resource_name']]['rid'];
        drupal_write_record('drealty_resource_mappings', $resource_mapping);
      }

      if (isset($entity->field_fields)) {
        foreach ($entity->field_fields as $field) {
          if (!field_read_field($field['field_name'], array('include_inactive' => TRUE))) {
            field_create_field($field);
          } else {
            drupal_set_message(t('Skipped creating field %field. A field with this name already exists.', array('%field' => $field['field_name'])), 'warning');
          }
        }
      }

      if (isset($entity->field_instances)) {
        foreach ($entity->field_instances as $bundle) {
          foreach ($bundle as $instance) {
            if (!field_read_instance($instance['entity_type'], $instance['field_name'], $instance['bundle'])) {
              field_create_instance($instance);
            } else {
              drupal_set_message(t('Skipped creating instance %instance. An instance with this name already exists.', array('%field' => $instance['label'])), 'warning');
            }
          }
        }
      }

      foreach ($entity->field_mappings as &$field_mapping) {
        $field_mapping['conid'] = $entity->conid;
        $field_mapping['rid'] = $resources[$field_mapping['resource_name']]['rid'];
        $field_mapping['cid'] = $classes[$field_mapping['class_name']]['cid'];
        drupal_write_record('drealty_field_mappings', $field_mapping);
      }
    }
  }

}

class drealty_no_active_connection_exception extends Exception {
  
}

class drealty_rets_disconnect_exception extends Exception {
  
}

class drealty_rets_connection_exception extends Exception {
  
}
