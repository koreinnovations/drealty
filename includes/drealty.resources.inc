<?php

class drealtyResources {

  private $defaultCache = 'cache';

  function __construct() {
    drealty_load_phrets();
  }

  function GetActiveFields($conid, $resource) {
    $cache_name = "drealty_fields_active_{$resource}_{$conid}";
    $cached = cache_get($cache_name, $this->defaultCache);
    if ($cached) {
      $result = db_query("SELECT * FROM {drealty_fields} WHERE conid = :conid AND resource = :resource AND display = 1", array(':conid' => $conid, ':resource' => $resource))->fetchAll();
      $fields = array();
      foreach ($result as $field) {
        $fields[$field->systemname] = $field;
      }
      cache_set($cache_name, $fields);
      return $fields;
    } else {
      return $cached->data;
    }
  }

  /**
   * Retrieve resource mappings for a given connnection
   *
   * @param int $conid
   *  The connection you want to retrieve mappings for
   * @return array
   */
  function GetResourceMappings($conid) {
    $cache_name = "drealty_mappings_{$conid}";
    $cached = cache_get($cache_name, $this->defaultCache);
    if (!$cached) {
      $result = db_query("SELECT * FROM {drealty_resource_mappings} WHERE conid = :conid", array(':conid' => $conid))->fetchAll();
      $mappings = array();
      foreach ($result as $mapping) {
        $mappings[$mapping->entity_type] = $mapping;
      }
      cache_set($cache_name, $mappings);
    } else {
      $mappings = $cached->data;
    }
    return $mappings;
  }

  function FetchActiveFields($conid, $resource) {
    $cache_name = "drealty_fields_active_{$resource}_{$conid}";
    $cached = cache_get($cache_name, $this->defaultCache);
    if (!$cached) {
      $result = db_query("SELECT * FROM {drealty_fields} WHERE conid = :conid AND resource = :resource AND display = 1", array(':conid' => $conid, ':resource' => $resource))->fetchAll();
      $fields = array();
      foreach ($result as $field) {
        $fields[$field->systemname] = $field;
      }
      cache_set($cache_name, $fields, $this->defaultCache);
      return $fields;
    } else {
      return $cached->data;
    }
  }

  
  function FetchFieldMappings($conid, $resource = NULL, $class_id = NULL) {
    
    $query = db_select('drealty_field_mappings', 'm')
      ->fields('m')
      ->condition('conid', $conid);
    
    if(isset($resource)) {
      $query->condition('resource', $resource);
    }
    if(isset($class_id)) {
      $query->condition('cid', $class_id);
    }
    
    $mappings = $query->execute()->fetchAllAssoc('field_name');
    foreach($mappings as $mapping) {
      $mapping->data = unserialize($mapping->data);
    }
    return $mappings;
  }
//  
//  function FetchFieldMappings($conid, $resource = NULL, $class = NULL) {
//    $cache_name = "drealty_field_mapping_{$resource}_{$conid}";
//    $cached = cache_get($cache_name, $this->defaultCache);
//
//    if (!$cached) {
//
//      $query = db_select('drealty_field_mappings', 'dfm')
//        ->fields('dfm')
//        ->condition('conid', $conid);
//
//      if (isset($resource)) {
//        $ra = db_and();
//        $ra->condition('resource', $resource);
//        $query->condition($ra);
//      }
//      if (isset($class)) {
//        $ca = db_and();
//        $ca->where('cid = :cid', array(':cid' => $class));
//        $query->condition($ca);
//      }
//
//      $results = $query->execute()->fetchAll();
//
//      $mappings = array();
//      foreach ($results as $result) {
//        $mappings[$result->field_name] = $result;
//      }
//      if (count($mappings)) {
//        cache_set($cache_name, $mappings, $this->defaultCache);
//      }
//      return $mappings;
//    } else {
//      return $cached->data;
//    }
//  }  

  function FetchFields($conid, $resource, $flush = FALSE) {
    $cache_name = "drealty_fields_{$resource}_{$conid}";
    $cached = cache_get($cache_name, $this->defaultCache);

    if (!$cached) {
      $result = db_query("SELECT * FROM {drealty_fields} WHERE conid = :conid AND resource = :resource", array(':conid' => $conid, ':resource' => $resource))->fetchAll();
      $fields = array();
      foreach ($result as $field) {
        $fields[$field->systemname] = $field;
      }
      if (count($fields) && !$flush) {
        cache_set($cache_name, $fields, $this->defaultCache);
      }
    } else {
      $fields = $cached->data;
    }

    $rets = new drealtyConnection();

    if ((!count($fields) || $flush) && $rets->connect($conid)) {
      $classes = $this->FetchClasses($conid, $resource);
      $rets_fields = array();

      foreach ($classes as $class) {

        $results = $rets->get_phrets()->GetMetadata($resource, $class->systemname);
        foreach ($results as $field) {
          $field = (object) $field;
          if (!isset($rets_fields[$field->SystemName])) {

            $newField = new stdClass();
            $newField->conid = $conid;
            $newField->resource = $resource;
            $newField->systemname = $field->SystemName;
            $newField->standardname = $field->StandardName;
            $newField->longname = $field->LongName;
            $newField->datatype = $field->DataType;
            $newField->max_length = $field->MaximumLength;
            $newField->field_precision = $field->Precision === '' ? 0 : $field->Precision;
            $newField->interpretation = $field->Interpretation;
            $newField->classes = array($class->systemname);
            $newField->lastupdate = strtotime("now");
            $newField->field_name = NULL;
            $newField->mapped_field = 0;
            $newField->display = 0;




            $rets_fields[$field->SystemName] = $newField;

            if (!empty($field->LookupName)) {
              $rets_fields[$field->SystemName]->lookupname = array($class->systemname => $field->LookupName);
            }
          } else {
            $rets_fields[$field->SystemName]->classes[] = $class->systemname;
            // check to see if the lookup table is different
            if (!empty($field->LookupName) && !isset($rets_fields[$field->SystemName]->lookupname[$class->systemname])) {
              $rets_fields[$field->SystemName]->lookupname[$class->systemname] = $field->LookupName;
            }
          }
        }
      }

      $rets->disconnect();
      if ($flush) {
        db_delete('drealty_fields')
          ->condition('conid', $conid)
          ->condition('resource', $resource)
          ->execute();
      }

      foreach ($rets_fields as &$field) {
        $field->classes = implode(',', $field->classes);
        if (!empty($field->lookupname)) {
          $field->lookupname = serialize($field->lookupname);
        }

        if (!$flush && isset($fields[$field->systemname]->fid)) {
          $field->fid = $fields[$field->systemname]->fid;
          drupal_write_record('drealty_fields', $field, fid);
        } else {
          drupal_write_record('drealty_fields', $field);
        }
      }
      $fields = $rets_fields;
      cache_set($cache_name, $fields, $this->defaultCache);
    }
    return $fields;
  }

  function FetchClass($cid) {
    $result = db_query('SELECT * FROM {drealty_classes} WHERE cid = :id', array(':id' => $cid));
    return $result->fetchObject();
  }

  /**
   * Retrive all MLS Class Definitions for the connection and resource.
   * @param int $conid
   * @param string $resource
   * @param bool $update
   *  Setting to TRUE will flush all classes and force an update from the RETS server
   * @return array
   */
  function FetchClasses($conid, $resource, $update = FALSE) {
    $cache_name = "drealty_classes_{$resource}_{$conid}";
    $cached = cache_get($cache_name, $this->defaultCache);
    $resources = $this->FetchResources($conid, $update);

    if (!$cached) {
      $result = db_query("SELECT * FROM {drealty_classes} WHERE conid = :conid AND rid = :rid", array(':conid' => $conid, ':rid' => $resources[$resource]->rid))->fetchAll();
      $classes = array();
      foreach ($result as $class) {
        $classes[$class->systemname] = $class;
      }
      if (count($classes) && !$update) {
        cache_set($cache_name, $classes, $this->defaultCache);
      }
    } else {
      $classes = $cached->data;
    }

    $rets = new drealtyConnection();

    if ((!count($classes) || $update) && $rets->connect($conid)) {

      $rets_classes = array();

      if (function_exists('date_default_timezone_set')) {
        date_default_timezone_set('UTC');
      }



      $results = $rets->get_phrets()->GetMetadataClasses($resource);
      foreach ($results as $class) {
        $class = (object) $class;
        $newClass = new stdClass();
        $newClass->cid = isset($classes[$class->ClassName]->cid) ? $classes[$class->ClassName]->cid : NULL;
        $newClass->systemname = $class->ClassName;
        $newClass->conid = $conid;
        $newClass->standardname = $class->StandardName;
        $newClass->visiblename = $class->VisibleName;
        $newClass->description = $class->Description;
        $newClass->rid = $resources[$resource]->rid;
        $newClass->lastupdate = NULL;
        $newClass->lifetime = 28800;
        $newClass->enabled = FALSE;
        $rets_classes[$class->ClassName] = $newClass;
      }
      $rets->disconnect();


      foreach ($rets_classes as &$class) {
        if (!empty($class->cid)) {
          drupal_write_record('drealty_classes', $class, 'cid');
        } else {
          unset($class->cid);
          drupal_write_record('drealty_classes', $class);
        }
      }
      $classes = $rets_classes;
      cache_set($cache_name, $classes, $this->defaultCache);
    }

    return $classes;
  }

  /**
   * Retrieve the current resources for a given connection.
   *
   * This will retrieve the cached resource list for a given connection. If
   * no resources are found in the cache it will attempt to load them from the db,
   * finding none there it will retrieve them from the RETS Server.
   *
   * @param int $conid
   *  Connection ID. The connection to retrieve the resources for.
   * @param bool $update
   *  Passing TRUE will cause the resources to be refreshed from the RETS server.
   * @return array()
   *  Returns and array() of resources.
   *
   */
  function FetchResources($conid, $update = FALSE) {
    $cache_name = "drealty_resources_$conid";
    $cached = cache_get($cache_name, $this->defaultCache);
    if (!$cached) {
      $result = db_query("SELECT * FROM {drealty_resources} WHERE conid = :conid", array(':conid' => $conid))->fetchAll();
      $resources = array();
      foreach ($result as $resource) {
        $resources[$resource->systemname] = $resource;
      }
      if (count($resources) && !$update) {
        cache_set($cache_name, $resources, $this->defaultCache);
      }
    } else {
      $resources = $cached->data;
    }

    $rets = new drealtyConnection();

    if ((!count($resources) || $update) && $rets->connect($conid)) {
      $rets_resources = array();

      $results = $rets->get_phrets()->GetMetadataResources();
      if (function_exists('date_default_timezone_set')) {
        date_default_timezone_set('UTC');
      }
      foreach ($results as $resource) {
        $resource = (object) $resource;
        $newResource = new stdClass();
        $newResource->rid = isset($resources[$resource->StandardName]->rid) ? $resources[$resource->StandardName]->rid : NULL;
        $newResource->conid = $conid;
        $newResource->systemname = $resource->ResourceID;
        $newResource->description = $resource->Description;
        $newResource->lastupdate = strtotime($resource->ClassDate);
        $newResource->keyfield = $resource->KeyField;


        $rets_resources[$resource->ResourceID] = $newResource;
      }

      $rets->disconnect();

      foreach ($rets_resources as &$resource) {
        if (!empty($resource->rid)) {
          drupal_write_record('drealty_resources', $resource, 'rid');
        } else {
          unset($resource->rid);
          drupal_write_record('drealty_resources', $resource);
        }
      }
      $resources = $rets_resources;
      cache_set($cache_name, $resources, $this->defaultCache);
    }
    return $resources;
  }

  function get_correlation_mls_system_name($correlation_name, $resource, $conid) {
    $result = db_result(db_query("SELECT systemName FROM {drealty_fields} WHERE correlation = '%s' AND resource = '%s' AND conid = %d", $correlation_name, $resource, $conid));
    if (!empty($result)) {
      return $result;
    } else {
      return FALSE;
    }
  }

  function get_correlation_field_name($correlation_name, $resource, $conid) {
    $result = db_result(db_query("SELECT cck_field_name FROM {drealty_fields} WHERE correlation = '%s' AND resource = '%s' AND conid = %d", $correlation_name, $resource, $conid));
    if (!empty($result)) {
      return $result;
    } else {
      return FALSE;
    }
  }

  function RemoveFields($conid) {

    db_delete("drealty_fields")
      ->condition("conid", $conid)
      ->execute();
    cache_clear_all("drealty_fields_", $this->defaultCache, TRUE);
  }

  function RemoveClasses($conid) {
    db_delete("drealty_classes")
      ->condition("conid", $conid)
      ->execute();
    cache_clear_all("drealty_classes_", $this->defaultCache, TRUE);
  }

  function RemoveResources($conid) {
    db_delete("drealty_resources")
      ->condition("conid", $conid)
      ->execute();
    cache_clear_all("drealty_resources_", $this->defaultCache, TRUE);
  }

  function RemoveFieldMappings($conid) {
    db_delete("drealty_field_mappings")
      ->condition("conid", $conid)
      ->execute();
    cache_clear_all("drealty_field_mapping_", $this->defaultCache, TRUE);
  }

  function RemoveResourceMappings($conid) {
    db_delete("drealty_resource_mappings")
      ->condition("conid", $conid)
      ->execute();
    cache_clear_all("drealty_mappings_", $this->defaultCache, TRUE);
  }

}