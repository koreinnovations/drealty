<?php

class dRealtyConnection {

  public function __construct() {
    drealty_load_phrets();
  }

  /**
   *
   * @param int $conid
   * @return bool
   */
  public function connect($conid) {

    // check to see if we  are connected to a rets server. If there is a connection
    // do nothing.

    if (!self::$_is_connected) {
      // need to setup a connection
      if (self::get_current_connection()) {
        try {
          return $this->handle_connection();
        } catch (drealty_rets_connection_exception $e) {
          watchdog('drealty', $e->getMessage(), NULL, WATCHDOG_ERROR);
          return FALSE;
        }
      } else {

        $connection = entity_load('drealty_connection_entity', array($conid));
//        // no connection setup need to create it.
//        $connections = $this->FetchConnections();
//        if (isset($conid)) {
//          if (count($connections) <= 0) {
//            throw new Exception("No Connections setup.");
//          }
//          $active_connection = $connections[$conid];
//        } else {
//          throw new Exception("Connection ID can not be null.");
//        }
//
//        $new_connection = new dRealtyConnectionEntity();
//
//        $new_connection->login_url = $active_connection->login_url;
//        $new_connection->username = $active_connection->username;
//        $new_connection->password = $active_connection->password;
//        $new_connection->force_basic_auth = $active_connection->force_basic_auth;
//        $new_connection->use_compression = $active_connection->use_compression;
//        $new_connection->ua_string = $active_connection->ua_string;
//        $new_connection->version = $active_connection->version;
        // set the connection

        self::set_current_connection(array_shift($connection));


        $error = FALSE;
        // force basic authentication
        if (self::get_current_connection()->force_basic_auth) {
          self::get_phrets()->SetParam('force_basic_authentication', TRUE);
        }
        // enable compression
        if (self::get_current_connection()->use_compression) {
          self::get_phrets()->SetParam('compression_enabled', TRUE);
        }

        if (self::get_current_connection()->use_interealty_auth) {
          self::get_phrets()->SetParam('use_interealty_ua_auth', TRUE);
        }

        //set debug mode
//        self::get_phrets()->SetParam('debug_mode', true);
//        self::get_phrets()->SetParam("debug_file", "debug_log.txt");
        // add some headers
        self::get_phrets()->AddHeader("User-Agent", self::get_current_connection()->ua_string);
        self::get_phrets()->AddHeader("RETS-Version", "RETS/" . self::get_current_connection()->version);

        // clean up
        unset($new_connection);
        unset($ative_connection);
        unset($connections);

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
    $current_connection = self::get_current_connection();
    if (!self::get_phrets()->Connect(self::get_current_connection()->login_url, self::get_current_connection()->username, self::get_current_connection()->password, self::get_current_connection()->ua_password)) {
      // we didn't connect, check to see if we have any error results
      if (self::get_phrets()->Error()) {
        // error result present, use that in our exception
        $error_info = self::get_phrets()->Error();
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
    if (self::get_phrets() && self::$_is_connected) {
      if (self::get_phrets()->Disconnect()) {
        self::$_is_connected = FALSE;
        drush_log("Disconnected from RETS server.");
        return TRUE;
      } else {
        throw new drealty_rets_disconnect_exception('Error Disconnecting from the RETS Server.');
      }
    }
  }

  /**
   * Signleton Design Pattern
   *  @link http://en.wikipedia.org/wiki/Singleton_pattern#PHP
   * @return phRETS
   */
  public static function get_phrets() {
    if (!self::$_phrets) {
      self::$_phrets = new phRETS();
    }
    return self::$_phrets;
  }

  /**
   * Set the current connection
   *
   * @param dRealtyConnectionEntity $connection
   */
  protected static function set_current_connection(dRealtyConnectionEntity $connection) {
    self::$_current_connection = $connection;
  }

  /**
   * Get a reference to the current connection
   *
   * @return dRealtyConnectionEntity
   */
  protected static function get_current_connection() {
    if (!self::$_current_connection) {
      return NULL;
    }
    return self::$_current_connection;
  }

  public $error_string = NULL;
  // the phRETS class
  protected static $_phrets = NULL;
  // wether we are connected to the RETS server
  protected static $_is_connected = FALSE;
  // the current RETS Connection credentials
  protected static $_current_connection = NULL;

  public function FetchConnections() {
    return entity_load('drealty_connection_entity');
  }

  public function FetchConnection($id) {
    return entity_load('drealty_connection', $id);
  }

}

class dRealtyListingEntity extends Entity {

  public $id;
  public $ListingId;
  public $ListingKey;
  public $ListPrice;
  public $UnparsedAddress;
  public $BathsFull;

  public function __construct($values = array()) {
    parent::__construct($values, 'drealty_listing');
  }

  protected function defaultLabel() {
    return $this->ListingId;
  }

}

class dRealtyConnectionEntity extends Entity {

  public $conid;
  public $name;
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

  public function __construct($values = array()) {
    parent::__construct($values, 'drealty_connection_entity');
  }

  protected function defaultLabel() {
    return $this->name;
  }

  public function label() {
    return $this->name;
  }

  public function save() {
    parent::save();

    // check to see if we are importing anything
    if (isset($this->resources)) {
      foreach ($this->resources as $resource) {
        $resource['rid'] = NULL;
        $resource['conid'] = $this->conid;
        drupal_write_record('drealty_resources', $resource);
      }
    }

    if (isset($this->resource_mappings)) {
      foreach ($this->resource_mappings as $resource_mapping) {
        $resource_mapping['mid'] = NULL;
        $resource_mapping['conid'] = $this->conid;
        drupal_write_record('drealty_resource_mappings', $resource_mapping);
      }
    }

    if (isset($this->classes)) {
      foreach ($this->classes as $class) {
        $class['cid'] = NULL;
        $class['conid'] = $this->conid;
        drupal_write_record('drealty_classes', $class);
      }
    }

    if (isset($this->fields)) {
      foreach ($this->fields as $field) {
        $field['fid'] = NULL;
        $field['conid'] = $this->conid;
        drupal_write_record('drealty_fields', $field);
      }
    }

    if (isset($this->field_mappings)) {
      foreach ($this->field_mappings as $field_mapping) {
        $field_mapping['conid'] = $this->conid;
        drupal_write_record('drealty_field_mappings', $field_mapping);
      }
    }
  }

//end save

  public function Resources() {
    $dr = new drealtyResources();
    return $dr->FetchResources($this->conid);
  }

  public function ResourceMappings() {
    $dr = new drealtyResources();
    return $dr->GetResourceMappings($this->conid);
  }

  public function FetchClasses($resource = 'Property') {
    $dr = new drealtyResources();
    return $dr->FetchClasses($this->conid, $resource);
  }

  public function FetchFields($resource = 'Property') {
    $dr = new drealtyResources();
    return $dr->FetchFields($this->conid, $resource);
  }

  public function FetchFieldMappings($resource = 'Property', $class = NULL) {
    $dr = new drealtyResources();
    return $dr->FetchFieldMappings($this->conid, $resource, $class);
  }

  public function CreateFieldMapMapping($resource, $field_name, $systemname, $class) {
    // remove any mapping and create a new one
    $this->RemoveFieldMapping($resource, $field_name, $class);

    db_insert('drealty_field_mappings')
      ->fields(array(
          'conid' => $this->conid,
          'resource' => $resource,
          'field_name' => $field_name,
          'systemname' => $systemname,
          'cid' => $class,
      ))
      ->execute();
  }

  public function RemoveFieldMapping($resource, $field_name, $class) {
    db_delete("drealty_field_mappings")
      ->condition("conid", $this->conid)
      ->condition("resource", $resource)
      ->condition("field_name", $field_name)
      ->condition('cid', $class)
      ->execute();
  }

  public function ExportData() {

    // fill in the resource junk
    $this->resources = $this->Resources();
    $this->resource_mappings = $this->ResourceMappings();
    $this->classes = $this->FetchClasses();
    $this->fields = $this->FetchFields();
    $this->field_mappings = $this->FetchFieldMappings(NULL, NULL);
  }

  public function delete() {
    $dr = new drealtyResources();

    $dr->RemoveFields($this->conid);
    $dr->RemoveFieldMappings($this->conid);
    $dr->RemoveClasses($this->conid);
    $dr->RemoveResourceMappings($this->conid);
    $dr->RemoveResources($this->conid);
    parent::delete();
  }

}

/**
 * The Controller for Model entities
 */
class dRealtyConnectionEntityContoller extends EntityAPIControllerExportable {

  public function __construct($entityType) {
    parent::__construct($entityType);
  }

  public function create(array $values = array()) {
    // Add values that are specific to our Model
    $values += array(
        'conid' => '',
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
    );
    $dRealtyConnectionEntity = parent::create($values);
    return $dRealtyConnectionEntity;
  }

  public function export($entity, $prefix = '') {
    $entity->ExportData();
    $vars = get_object_vars($entity);
    unset($vars[$this->statusKey], $vars[$this->moduleKey], $vars['is_new']);
    if ($this->nameKey != $this->idKey) {
      unset($vars[$this->idKey]);
    }
    return entity_var_json_export($vars, $prefix);
  }

}

class drealty_no_active_connection_exception extends Exception {
  
}

class drealty_rets_disconnect_exception extends Exception {
  
}

class drealty_rets_connection_exception extends Exception {
  
}