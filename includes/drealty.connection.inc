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
          if($path && !empty($path)) {
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
        drush_log("Disconnected from RETS server.");
        return TRUE;
      } else {
        throw new drealty_rets_disconnect_exception('Error Disconnecting from the RETS Server.');
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

  public function export($entity, $prefix = '') {
    $entity->active = 0;

    
    

    
    
  }

}

class drealty_no_active_connection_exception extends Exception {
  
}

class drealty_rets_disconnect_exception extends Exception {
  
}

class drealty_rets_connection_exception extends Exception {
  
}