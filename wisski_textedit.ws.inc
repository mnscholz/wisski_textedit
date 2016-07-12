<?php




/* Extract the query parameters for a request
*
* Provide an array $query of parameter keys with default values.
* If a key is found, it will be set in $query.
* 
* @return an array with all paramteres found (not only those defined in $query)
*
* All Web Services use the same mechanism to retrieve parameters.
* For backwards compatibility we provide a series of http parameters where
* a parameter object may be set: wisski_textedit_data, wisski_query, textdata
* The parameters can also be encoded as http params. Provide a $key to detect
* whether this is the case.
* The last option is to parse the request body (if present)
*
* @author Martin Scholz
*
*/
function _wisski_textedit_ws_query_params(&$query, $key = NULL) {

#var_dump($_GET);
#var_dump($_POST);
#var_dump($_REQUEST);
  $q = (object) NULL;
  if (isset($_POST['wisski_editor_query'])) {
    $q = $_POST['wisski_editor_query'];
    $q = json_decode($q);
#var_dump(1);
  } elseif (isset($_GET['wisski_editor_query'])) {
    $q = $_GET['wisski_editor_query'];
    $q = json_decode($q);
#var_dump(2);
  } elseif (isset($_POST['wisski_query'])) {
    $q = $_POST['wisski_query'];
    $q = json_decode($q);
#var_dump(3);
  } elseif (isset($_GET['wisski_query'])) {
    $q = $_GET['wisski_query'];
    $q = json_decode($q);
#var_dump(4);
  } elseif (isset($_POST['textdata'])) {
    $q = $_POST['textdata'];
    $q = json_decode($q);
  } elseif (isset($_GET['textdata'])) {
    $q = $_GET['textdata'];
    $q = json_decode($q);
  } elseif ($key !== NULL && isset($_POST[$key])) {
    $q = (object) $_POST;
  } elseif ($key !== NULL && isset($_GET[$key])) {
    $q = (object) $_GET;
  } else {
#var_dump(9);
    $data = file_get_contents("php://input");
    if ($data) {
      $q = json_decode($data);
    }
  }
#var_dump($q);

  foreach ($query as $k => $v) {
    if (isset($q->$k)) $query->$k = $q->$k;
  }
  
  return $q;

}


/* Web service callback: Suggest vocabulary entries for a term
* 
* See below for a list of valid parameters
*
* @author Martin Scholz
*
*/
function wisski_textedit_ws_suggest() {
  
// TODO uncomment  drupal_set_header('Content-Type: application/json; charset=utf-8');
  $q->term = NULL; // the term to search for
  $q->limit = NULL; // maximum number of results
  $q->offset = NULL; // offset in result list. use together with limit to query "pages"
  $q->match_mode = 'exact'; // the match mode, see wisski_vocab_ctrl_lookup() for possible values
  $q->fields = '';  // which fields of result entries should be sent back (ws-separated list)
  $q->vids = '';  // a list of vocab ids to search in, empty means all vocabs (ws-separated list)
  $q->id = NULL; // if supplied it will be added to the return object for identification
  _wisski_textedit_ws_query_params($q);

  // The term parameter is mandatory
  if ($q->term === NULL) {
    drupal_set_header('HTTP/1.1 400 Bad Request');
    print json_encode(array("error" => "no valid query"));
    return;
  }

  if (!$q->fields) $q->fields = array();
  else $q->fields = preg_split('/\\s+/', $q->fields);
  
  drupal_load('module', 'wisski_vocab_ctrl');
  if (empty($q->vids)) {
    // empty list means all vocabs
    $vocs = wisski_vocab_ctrl_get();
  } else {
    $vids = preg_split('/\\s+/', $q->vids);
    $vocs = array();
    foreach ($vids as &$vid) if ($vid) $vocs[] = wisski_vocab_ctrl_get($vid);
    usort($vocs, function($a, $b) { return $a->priority - $b->priority; });
  }
  
  // make the lookup voc per voc
  $uris_per_voc = array();
  foreach ($vocs as $voc) {
    
    $uris = wisski_vocab_ctrl_lookup('labels', $voc, NULL, $q->term, $q->limit, $q->offset, $q->match_mode, 'normal');
    
    if ($uris) {
      
      if ($q->fields) {
        // provide the requested fields        
        foreach ($uris as $uri => &$vals) {
          $vals = wisski_vocab_ctrl_lookup($q->fields, $voc, $uri, NULL, NULL, NULL, 'exact', 'never'); // never use an indexed value
          $vals = isset($vals[$uri]) ? $vals[$uri] : array();
        }
      
      }
      
      $uris_per_voc[$voc->vid] = $uris;
    
    }

  }
  
  // add the request id if needed
  if ($q->id !== NULL) $uris_per_voc['id'] = $q->id;
  
  // print results
  print json_encode($uris_per_voc);
  exit();
  
}


/* Web service callback: Return information about a vocab entry
* 
* See below for a list of valid parameters
*
* @author Martin Scholz
*
*/
function wisski_textedit_ws_about() {
  
// TODO uncomment  drupal_set_header('Content-Type: application/json; charset=utf-8');
  
  $q->uri = NULL; // the uri of the entry
  $q->fields = 'all'; // the requested fields, see wisski_vocab_ctrl_lookup() for valid field names
  $q->vids = '';  // the vocabs to search in (ws-separated list of ids).
                  // If the vocab is known, it should be set for performance reasons; if left empty, all vocabs are searched!
  $q->id = NULL;  // a request id that is sent back with the results (optional)
  _wisski_textedit_ws_query_params($q);
  
  // uri is mandatory
  if (empty($q->uri)) {
    drupal_set_header('HTTP/1.1 400 Bad Request');
    print json_encode(array("error" => "no valid query"));
    return;
  }
  // there must be at least one field to retrieve
  if (empty($q->fields)) {
    drupal_set_header('HTTP/1.1 400 Bad Request');
    print json_encode(array("error" => "no fields specified"));
    return;
  }
  $q->fields = preg_split('/\\s+/', $q->fields);
  
  drupal_load('module', 'wisski_vocab_ctrl');
  if (!isset($q->vids) || !$q->vids) {
    // get all vocabs
    $vocs = wisski_vocab_ctrl_get();
  } else {
    $vids = preg_split('/\\s+/', $q->vids);
    $vocs = array();
    foreach ($vids as &$vid) if ($vid) $vocs[] = wisski_vocab_ctrl_get($vid);
    usort($vocs, function($a, $b) { return $a->priority - $b->priority; });
  }

  // do the lookup voc per voc
  $uris_per_voc = array();
  foreach ($vocs as $voc) {
   
    $vals = wisski_vocab_ctrl_lookup($q->fields, $voc, $q->uri, NULL, NULL, NULL, 'exact', 'never');
     
    if ($vals) $uris_per_voc[$voc->vid] = $vals;
    
  }
  
  // add request id
  if ($q->id) $uris_per_voc['id'] = $q->id;
  
  // print results
  print json_encode($uris_per_voc);
  exit();
  
}



/** Create the CSS file for annotations
* defunct: Does not work in D6! CSS files may not be created on the fly
*
* @author Martin Scholz
*
*/
function wisski_textedit_ws_css() {
  
  drupal_set_header('Content-Type: text/css; charset=utf-8');
  
  print file_get_contents(drupal_get_path('module', 'wisski_textedit') . '/common.css') . "\n";

  $classes_css = file_get_contents(drupal_get_path('module', 'wisski_textedit') . '/classes.css');
  $ontology = wisski_textedit_build_ontology();
  $classes = $ontology['classes'];
  foreach ($classes as $id => $info) {
    $replace = array(
      '%<class>' => $id,
      '%<class_name>' => $info['name'],
    );
    foreach (variable_get("wisski_textedit_class_settings_$id", array()) as $k => $v) {
      $replace[$k] = $v;
    }
    print strtr($classes_css, $replace) . "\n";
  }

  exit();
 
}


