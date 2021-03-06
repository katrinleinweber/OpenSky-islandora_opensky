<?php
/**
 * Assumes PID is last segment of request path
 * e.g., https://osstage2cl.dls.ucar.edu/islandora/object/technotes:546
 */
function opensky_get_pid_from_request () {
    $pat = 'islandora/object';

    if (substr (request_path(), 0, strlen($pat)) != $pat) {
        return null;
    }
    $path_parts = explode('/', request_path());
    $tail = array_pop($path_parts);
    $parts = explode (':', $tail);
    if (count($parts) == 2) {
        return $tail;
    }
    return null;
}

function opensky_get_search_term_from_request() {
    $pat = ISLANDORA_SOLR_SEARCH_PATH;

    $path = request_path();

    if (strlen($path) <= strlen($pat) || substr (request_path(), 0, strlen($pat)) != $pat) {
        return null;
    }

    $parts = explode('/',request_path());
    $term = array_pop($parts);
    //    $term = array_pop(explode('/',request_path()));
    return trim($term);
}

function opensky_truncate($str) {
    $truncated = truncate_utf8($str, 50, $wordsafe=TRUE);
    if (strlen ($str) != strlen($truncated)) {
        $truncated = "$truncated ...";
    }
    return $truncated;
}

/** 
 * Get the fedora object for provided pid.
 */
function opensky_get_fedora_object($pid) {
    $my_islandora_tuque = new IslandoraTuque();
    $repository = $my_islandora_tuque->repository;
    try {
        return $repository->getObject($pid);
    } catch (Exception $e) {
        // dsm ('object ('.$pid.') not in repository');
        return null;
    }
}

/**
 * return array representing DC metadata for provided $pid, or null if not found 
 */
function opensky_get_dc_array ($pid) {
    $fedora_object = opensky_get_fedora_object($pid);
    if ($fedora_object == null) {
        // dsm ('object ('.$pid.') not in repository');
        return null;
    }

    $dc = $fedora_object['DC'];
    $dc_object = DublinCore::importFromXMLString($dc->content);
    $dc_array = $dc_object->asArray();
    // $title = $dc_array['dc:title']['value'];
    return $dc_array;
}


/**
 *
 */
function opensky_get_pid_subcollections_pids ($parent_pid) {

    $key = 'subcollections_pids_'.$parent_pid;
    if ($cache = cache_get($key)) {
        $pids = $cache->data;
    }
    else {
        $obj = opensky_get_fedora_object($parent_pid);
        if ($obj != null) {
            $pids =  opensky_get_obj_subcollection_pids($obj);
        } else {
            $pids = array();
        }

        cache_set ($key, $pids);
    }
    return $pids;
}

/**
 * Gets a list of ALL subcollections under parent collection (by recursively calling 
 * $opensky_get_subcollections.
 *
 * @param string $parent_pid
 *   The parent collection object.
 *
 * @return array
 *   The list of child PIDs.
 */

function opensky_get_subcollections_recursive($parent_pid){

    $key = 'subcollections_recursive_'.$parent_pid;

    if ($cache = cache_get($key)) {
        $subs = $cache->data;
    } else {
        $subs = array($parent_pid);
        $children = opensky_get_pid_subcollections_pids($parent_pid);
        if (count($children)) {
            foreach ($children as $child) {
                $subs = array_merge($subs, opensky_get_subcollections_recursive($child));
            }
        }
        cache_set($key, $subs);
    }

    return $subs;
    
}

/**
 * Gets a list of all immediate child PIDs.
 *
 * @param AbstractObject $object
 *   The parent collection object.
 *
 * @return array
 *   The list of child PIDs.
 */
function opensky_get_obj_subcollection_pids(AbstractObject $object) {
    $get_pid = function($o) {
        return $o['pid'];
    };

    return array_map($get_pid, opensky_get_obj_subcollection_infos ($object));
}

function opensky_is_item_pid($pid) {
    $suffix = explode(':', $pid)[1];
    return $suffix != '*' && is_numeric($suffix);
}

function opensky_is_collection_pid ($pid) {
    $suffix = explode(':', $pid)[1];
    return $suffix != '*' && !is_numeric($suffix);
}


function opensky_has_subcollections ($pid) {
    return (count(opensky_get_pid_subcollections_pids($pid)) > 0);
}

/**
 * Returns list of opensky_collection_info objects:
 * - pid
 * - title
 */
/**
 * Gets a list of all immediate child PIDs.
 *
 * @param AbstractObject $object
 *   The parent collection object.
 *
 * @return array
 *   The list of opensky_collection_info objects:
 *    - pid
 *    - title
 */
function opensky_get_obj_subcollection_infos(AbstractObject $object) {
    module_load_include('inc', 'islandora_basic_collection', 'includes/utilities');

    $params = array(
        'object' => $object,
        'page_size' => -1,
        'model' => "<info:fedora/islandora:collectionCModel>",
    );

    $map_results = function($o) {
        return array (
            'pid' => $o['object']['value'],
            'uri' => $o['object']['uri'],
            'title' => $o['title']['value'],
        );
    };
    $query_info = islandora_basic_collection_get_query_info($params);
    $results = $object->repository->ri->query($query_info['query'], $query_info['type']);
    return array_map($map_results, $results);
}

function opensky_get_pid_subcollection_infos ($parent_pid) {

    $key = 'subollection_infos_'.$parent_pid;

    if ($cached = cache_get($key)) {
        $infos = $cached->data;
    } else {
        $obj = opensky_get_fedora_object($parent_pid);
        if ($obj != null) {
            $infos = opensky_get_obj_subcollection_infos($obj);
        }
        else {
            $infos = array();
        }
        cache_set ($key, $infos);
    }

    return $infos;
}


/**
 * Create a mapping from each collection to the (non-recursive) subcollections under it.
 * Currently only used on scratch (demo) page but could be used to cache the tree
 * structure for faster lookup.
 * @return array
 */
function opensky_get_collection_tree () {
    $pid = 'opensky:root';
    $tree = array();
    foreach (opensky_get_subcollections_recursive('opensky:root') as $pid) {
        $tree[$pid] = opensky_get_pid_subcollections_pids($pid);
    }
    return $tree;
}

/**
 * GEt list of searchable subcollections, which are those subcollections (recursively)
 * that contain items (and not subcollections)
 *
 * @param $pid
 * @return array
 */
function opensky_get_searchable_subcollections ($pid) {
    $descendants = opensky_get_subcollections_recursive($pid);
    $tree = array();
    foreach($descendants as $child) {
        if (!opensky_has_subcollections($child)) {
            $tree[] = $child;
        }
    }
    return $tree;
}

/*
View this collection (identified by $pid) in search view.
Called in islandora_sulution_pack_collection/theme/theme.inc to redirect
collections to search view
*/
function opensky_redirect_to_search_view ($pid) {
    // dsm('opensky_redirect_to_search_view redirecting to search: '. $pid);
    $options = array('query' => array(
        'type'=>'dismax',
        'collection'=>$pid
    ));
    drupal_goto('/islandora/search/',$options);
}
/**
 * IMplements hook_islandora_solr_query
 *
 * HARD-CODE fields returned by solr query to make solr responses smaller                                                           
 * and thus avoid PHP Out of memory errors 
 */
function opensky_islandora_solr_query($islandora_solr_query) {
    //    dpm ('opensky_islandora_solr_query');
    //    dpm ($islandora_solr_query);

    $islandora_solr_query->solrParams['fl'] = 'PID,mods*,fgs*,dc.*,dsmd_DC*,keyDate*,RELS_EXT*,*Display*,fedora_datastreams_ms';

}

/**
 * $qp is an instance of IslandoraSolrQueryProcessor
 * if there is a collection parameter, add filters to limit search to named
 * collection and it's subcollections if any exist
 */
function opensky_islandora_solr_query_alter($qp) {
    // dsm('opensky_islandora_solr_query_alter');

    // if sort is not set for search pages, then set it to keyDate desc
    if (current_path() == ISLANDORA_SOLR_SEARCH_PATH  && !isset($qp->solrParams['sort'])) {
        $qp->solrParams['sort'] = 'keyDate desc';
    }

    if (isset($qp->internalSolrParams['collection'])) {
        $collection = $qp->internalSolrParams['collection'];

        // create $fq_new by removing all RELS_EXT_isMemberOfCollection_uri_ms
        // clauses
        $fq_new = array();
        $pat = 'RELS_EXT_isMemberOfCollection_uri_ms';
        if (isset($qp->solrParams['fq'])) {
            $fq_old = $qp->solrParams['fq'];
            unset($qp->solrParams['fq']);
            foreach ($fq_old as $item) {
                if (substr($item, 0, strlen($pat)) !== $pat) {
                    $fq_new[] = $item;
                }
            }
        }

        // construct a filter to search all searchable sub-collections (those
        // that have no sub-collections).
        $searchable_collections = opensky_get_searchable_subcollections($collection);

        $children_params = array();
        foreach ($searchable_collections as $searchable) {
            $children_params[] = 'RELS_EXT_isMemberOfCollection_uri_ms:"info:fedora/'.$searchable.'"';
        }

        $searchable_collection_clause = implode(' OR ', $children_params);

        $qp->solrParams['fq'] = array_merge($fq_new, array($searchable_collection_clause));
        // dsm($qp->solrParams);
    }
}

/**
 * implements hook_islandora_solr_results_alter
 *
 * Set thumbnail for citationCModel objects to OpenSky custom image
 * (BiblioCitOnly.png);
 */
function opensky_islandora_solr_results_alter(&$object_results, $query_processor) {

    $default_path = drupal_get_path('module', 'islandora_solr') . '/images/defaultimg.png';
    foreach ($object_results as $object_index => $object_result) {
        if (isset ($object_results[$object_index]['thumbnail_url']) && $object_results[$object_index]['thumbnail_url'] == $default_path) {
            if (strpos($object_results[$object_index]['content_models'][0], 'citationCModel') !== false) {
                $object_results[$object_index]['thumbnail_url'] = drupal_get_path('module', 'opensky') . '/images/BiblioCitOnly.png';
            }
        }
    }
}

/**
 * hook to insert the collection link and x_link into the breadcrumbs (see
 * islandora_solr_search/includes/results->setBreadcrumbs()

DON'T KNOW how to explain, but this function is getting called TWICE in a row.
You can bracket the drupal_alter call (see islandora_solr_search/includes/results.inc
getBreadcrumbs() with print statements and for whatever evil reason,
opensky_islandora_breadcrumbs_alter gets called TWICE between the prints!?!?

therefore, we have to detect when we're called the second time and
politely refuse to do anything .....
 */
function opensky_islandora_breadcrumbs_alter(&$data, $context) {
    // only make change if there is a collection param
    if (!isset($_GET['collection'])) {
        return;
    }
    $collection = $_GET['collection'];

    // Look for the collection_breadcrumb - it has a "collection-crumb" class so
    // we just look for that string
    foreach ($data as $crumb) {
        if (strpos($crumb, 'collection-crumb') !== false) {
            // our work is done - we've already added the collection breadcrumb
            return;
        }
    }
    $collection_breadcrumb = opensky_collection_breadcrumb($_GET);

    // splice it into existing breadcrums
    array_splice ($data, 1, 0, $collection_breadcrumb);
}


/**
 * creates the collection breadcrumb, which is inserted into
 * breadcrumbs by opensky_islandora_breadcrumbs_alter
 */
function opensky_collection_breadcrumb($params) {
    $collection = $params['collection'];     
    $dc_array = opensky_get_dc_array($collection);
    $filter_string = $dc_array['dc:title']['value'];
    
    // Remove filter array for collection breadcrumb link
    $query = array();
    
    $query_diff = $params;
    
    if (isset($query_diff['f'])) {
        unset($query_diff['f']);
    }
    
    // Set attributes variable.
    $attr = array();
    $attr['title'] = $filter_string;
    $attr['rel'] = 'nofollow';
    if (isset($exclude) && $exclude) {
        $attr['class'] = 'strikethrough';
    }
    // remove query from path if there is one
    // $attr['href'] = url(ISLANDORA_SOLR_SEARCH_PATH, array('query' => $query_diff));
    $attr['href'] = url('/islandora/search', array('query' => $query_diff));
        
    // Pull out filter (for x link).
    $query_x = array();  // query will be like current except it will not have collection param
    foreach ($params as $key => $value) {
        if ($key != 'collection' && $value == $collection) $query_x[$key] = $value;
    }

    $attr_x = array();
    $attr_x['title'] = t('Remove') . ' ' . $filter_string;
    $attr_x['rel'] = 'nofollow';
    $attr_x['href'] = url(current_path(), array('query' => $query_x));
    
    $breadcrumb = '<a' . drupal_attributes($attr) . '>' . check_plain($filter_string) . '</a>'
                . '<span class="islandora-solr-breadcrumb-super collection-crumb"> <a' . drupal_attributes($attr_x) . '>('
                . t('x') . ')</a></span>';
    return $breadcrumb;
}

/**
 * Supply in collections info for islandora_scholar (citation model).
Called from islandora_scholar_get_metadata_display(in islandora_scholar/includes/utilities.inc)
 */
function opensky_get_in_collections_display ($object) {
    $parent_collections = islandora_get_parents_from_rels_ext($object);
    return theme('opensky_in_collections', array(
        'parent_collections' => $parent_collections));
}

function opensky_retrieve_doi_markup(AbstractObject $object) {
    $markup = '';
    $mods_xml = simplexml_load_string($object['MODS']->content);
    $mods_xml->registerXPathNamespace('mods', 'http://www.loc.gov/mods/v3');
    $doi_xpath = $mods_xml->xpath('//mods:identifier[@type="doi"]');
    if ($doi_xpath) {
        $doi_link = (string) $doi_xpath[0];
        $markup = 'Published Version: <a href="http://dx.doi.org/' . $doi_link . '">http://dx.doi.org/' . $doi_link . '</a> <br/> <br/>';
    }
    return $markup;
}

/**
 * Add "in collection" link to metadata display.
 */
function opensky_islandora_scholar_get_metadata_display (&$display, $object, $weight) {

    if (variable_get('islandora_scholar_use_standard_metadata_display', FALSE)) {
        module_load_include('inc', 'islandora', 'includes/metadata');
        // Borrowing core JS for fieldsets.                                                                                              
        drupal_add_js('misc/form.js');
        drupal_add_js('misc/collapse.js');
        $metadata = islandora_retrieve_metadata_markup($object);
        $description = islandora_retrieve_description_markup($object);
        
        $doi_link = opensky_retrieve_doi_markup($object);

        // Insert 'in collections' display element from opensky                                                            
        $in_collections = opensky_get_in_collections_display($object);

        $display['#markup'] = $doi_link . $description . $in_collections . $metadata;
    }

}

/*
 * Islandora_scholar _pdf_upload_form_alter is used to upload PDF in the process
 * of creating a new object with forms. (see includes/pdf_upload.form.inc)

 * Here we simply remove the usage options from the form, and then replace the
 * scholar module's validation function
 * (islandora_scholar_pdf_upload_form_validate) with our own version that
 * doesn't require the usage options.
 */
function opensky_form_islandora_scholar_pdf_upload_form_alter (&$form, &$form_state, $form_id) {
    //    remove $form['upload_document']['usage']
    unset ($form['upload_document']['usage']);

    // now change the validation function
    module_load_include ('inc', 'opensky', 'includes/utilities');

    // insert opensky validate before islandora_scholar_pdf_upload_form_validate
    //    (didn't work)
    //    array_splice ($form['hidden_next']['#validate'], 0, 0, 'opensky_pdf_upload_form_validate'); 
  
    // replace islandora_scholar_pdf_upload_form_validate with opensky version
    $form['hidden_next']['#validate'][0] = 'opensky_pdf_upload_form_validate'; 
}

/*
 * This is a duplicate of islandora_scholar_pdf_upload_form_validate except we
 * don't validate the usage block, since we removed these fields from form (see
 * includes/pdf_upload.form.inc) 
 *
 * NOTE: I tried to do this in a more graceful way, but 
 */
function opensky_pdf_upload_form_validate(&$form, &$form_state) {
    error_log ('islandora_scholar_pdf_upload_form_validate ...');
    if ($form_state['values']['upload_pdf_checkbox']) {
        if (empty($form_state['values']['file'])) {
            form_set_error('file', t('A file must be uploaded!'));
        }
        if (empty($form_state['values']['version'])) {
            form_set_error('version', t('A document version must be selected!'));
        }

        /*
        // OpenSky - comment block
        if (empty($form_state['values']['usage'])) {
            form_set_error('usage', t('A usage permission must be selected!'));
        }
        // end opensky comment
        */

        if (empty($form_state['values']['certifying']['certify'])) {
            form_set_error('certifying', t('You must certify that you have the right to upload this PDF!'));
        }
    } 
}

//----------------
// Experimental - this is how we would alter ALL content model views 
function opensky_islandora_view_object_alter (&$object, &$rendered) {
    //    dpm ("VIEW_OBJECT_ALTER");
    //    dpm ($object->models[0]);
    //    dpm ($rendered);

}

/**
 * Alter all citationCModel views
 *
 * - view and download counts
 * - dowload PDF link
 * - "in collections" 
 */
function opensky_ir_citationCModel_islandora_view_object_alter (&$object, &$rendered) {

    // add viewed and downloaded counts if there is a citation.tab property in $rendered
    if (isset($rendered['citation.tab'])) {
        if (module_exists('islandora_usage_stats')) {
            module_load_include('inc', 'islandora_usage_stats', 'includes/db');
            // using the view_count method from db.inc
            $view_count = islandora_usage_stats_get_individual_object_view_count($object);
            $ds_count = islandora_usage_stats_get_datastream_downloads_count($object);
            $times_downloaded = 0;
            if (array_key_exists('PDF',$ds_count)){
                $times_downloaded = $ds_count['PDF'];
            }
            $rendered['citation.tab']['view_count']['#markup'] =t("<br/>Times Viewed on OpenSky: $view_count <br/>" .
                                                 "Times Downloaded on OpenSky: $times_downloaded");
            /* we have to assign view_count a weight that puts it in between
               'preview' and 'pdf_download'. the other #weight values are integers but the following seems to
               work to put 'view_count' above 'pdf_download'. note that using 'pdf_download's exact 
               weight also does the same thing ...
            */
            $rendered['citation.tab']['view_count']['#weight'] = $rendered['citation.tab']['pdf_download']['#weight'] - 0.5;
        } 
        
        // Set text on download link for PDFs (see original in islandora_scholar_get_view())
        
        $default_viewer = variable_get('islandora_scholar_viewers');
        $default_viewer = $default_viewer['default'];

        $default_viewer = 'none';  // hard-coded here because I can't find a way to configure so it is 'none'!

        if (isset($object['PDF']) && islandora_datastream_access(ISLANDORA_VIEW_OBJECTS, $object['PDF']) && $default_viewer == 'none') {
            $filename = str_replace(":", "_", $object->id);
            $rendered['citation.tab']['pdf_download'] = array(
                '#type' => 'item',
                '#markup' => t('Download ') . l(t('PDF'), "islandora/object/$object->id/datastream/PDF/download/citation.pdf"),
                '#weight' => $rendered['citation.tab']['pdf_download']['#weight'],
            );
        }

        // change the way "metadata" is displayed (see
        // islandora_scholar_get_metadata_display) to include $doi_link (Published Version ..) and
        // $in_collections
        if (variable_get('islandora_scholar_use_standard_metadata_display', FALSE)) {
            module_load_include('inc', 'islandora', 'includes/metadata');
            // Borrowing core JS for fieldsets.
            drupal_add_js('misc/form.js');
            drupal_add_js('misc/collapse.js');

            $metadata = islandora_retrieve_metadata_markup($object);
            $description = islandora_retrieve_description_markup($object);
            $doi_link = opensky_retrieve_doi_markup($object);
            $in_collections = opensky_get_in_collections_display($object);

            $rendered['citation.tab']['metadata']['#markup'] = $doi_link . $description . $in_collections . $metadata;
        }        
    }
}
// ---------- overriding scholar/includes/upload.tab.inc
/**
 * 
 * This form is accessed via the "Document" tab for a citation object (and others??). 
 *
The only thing we want to modify is:
$mods_versions = $mods_xpath->query('//m:mods/m:physicalDescription/m:note[@type="version"]');

The problem is that the alter function does not have immediate access to object and hence to 
the MODS Stream. We could get it via
*
*/
function opensky_form_islandora_scholar_modify_form_alter (array &$form, array &$form_state) {

    // grab object from form_state
    $object = $form_state['build_info']['args'][0];
    $pid = $object->id;
    //    dpm ("pid: $pid");
    
    // get the version from the opensky-specific location in the MODS stream
    $mods_str = trim($object['MODS']->content);

    $mods_doc = new DOMDocument();
    $mods_doc->loadXML($mods_str);
    $mods_xpath = new DOMXPath($mods_doc);
    $mods_xpath->registerNamespace('m', 'http://www.loc.gov/mods/v3');
    $mods_versions = $mods_xpath->query('//m:mods/m:physicalDescription/m:note[@type="version"]');
    
    if ($mods_versions->length > 0) {
        $version = $mods_versions->item(0)->textContent;
        $form['version']['#markup'] = $version;
    }
}

/**
 * Implements hook_islandora_scholar_upload_form_alter
 *
 * Just like in islandora_scholar_PDF_upload_form, we want to remove 
 * $form['upload_document']['usage'] from the form.
 * 
 * In this case, there is no validation function to modify, but we do replace
 * the submit function
*/
function opensky_form_islandora_scholar_upload_form_alter (&$form, &$form_state, $form_id) {
    unset ($form['upload_document']['usage']);

    // we also want to change an aspect of this form's submit process (see
    // islandora_scholar_upload_form_submit).

    // We replace islandora_scholar_upload_submit because we need to change the way it 
    // modifies the MODS with values from the form.

    foreach ($form['#submit'] as $key=>$submit_fn) {
        if ($submit_fn == 'islandora_scholar_upload_form_submit') {
            $form['#submit'][$key] = 'opensky_upload_form_submit';
        }}
}

/**

This version of the submit function modifies the MODS with
opensky_add_usage_and_version_elements_to_mods rather than
islandora_scholar_add_usage_and_version_elements_to_mods
*/
function opensky_upload_form_submit (array $form, array &$form_state) {
    extract($form_state['values']);

    $object = islandora_object_load($pid);

    try {
        $datastream = $object->constructDatastream($dsid, 'M');
        $datastream->label = 'PDF Datastream';
        $datastream->mimetype = 'application/pdf';
        $file = file_load($file);
        $path = drupal_realpath($file->uri);
        $datastream->setContentFromFile($path);

        $object->ingestDatastream($datastream);

        opensky_add_usage_and_version_elements_to_mods($object, $usage, $version);
    }
    catch (Exception $e) {
        drupal_set_message(t('@message', array('@message' => check_plain($e->getMessage()))), 'error');
    }
}

/**
 * Replaces islandora_scholar_add_usage_and_version_elements_to_mods to
 * implement opensky-specific ways of encoding usage and version values in MODS.
 */
function opensky_add_usage_and_version_elements_to_mods (AbstractObject $object, $usage, $version) {

    $refuse_msg = t('Refusing to add usage and publication version statements alone');
    if (empty($object['MODS'])) {
        drupal_set_message(t('No MODS in object @pid. %refuse.', array('@pid' => $object->id, '%refuse' => $refuse_msg)), 'error');
        return;
    }
    $namespace = 'http://www.loc.gov/mods/v3';
    $doc = new DOMDocument();
    $doc->loadXML($object['MODS']->content);
    $xpath = new DOMXPath($doc);
    $xpath->registerNamespace('mods', $namespace);
    // Remove all instances of mods:accessCondition and mods:physicalDescription
    // from every mods:mods element instance.
    // OpenSky change
    $physical_description_query = '//mods:physicalDescription';
    $results = $xpath->query("$physical_description_query");
    foreach ($results as $result) {
        $result->parentNode->removeChild($result);
    }
    // Regardless of the number of mods:mods elements in the document we only
    // add the usage and version to the first one.
    $results = $xpath->query('//mods:mods[1]');
    if ($results->length == 0) {
        drupal_set_message(t('Could not find MODS element in object @pid is empty. %refuse.', array('@pid' => $object->id, '%refuse' => $refuse_msg)), 'error');
        return;
    }
    $mods_element = $results->item(0);

    #  // Add mods:physicalDescription to the first mods:mods element.
    $physical_description_element = $doc->createElementNS($namespace, 'physicalDescription');

    $physical_description_note_element = $doc->createElementNS($namespace, 'note');
    $physical_description_note_element->setAttribute('type', 'version');
    $physical_description_note_element->nodeValue = $version;
    $physical_description_element->appendChild($physical_description_note_element);
    $mods_element->appendChild($physical_description_element);

    // Commit changes to datastream.
    $object['MODS']->setContentFromString($doc->saveXML());
}

/* ----- islandora_scholar_embargo module alterations

   The two functions below encapsulate the opensky modifications to the embargo
   module. 

   - opensky_form_islandora_scholar_embargo_form_alter introduces some "Embargo
     Options" beyond those supplied, and it also replaces
     islandora_scholar_embargo_form_submit with opensky_embargo_form_submit
     (which is a verbatim copy of the modified
     islandora_scholar_embargo_form_submit)

    - NOTE: the modified embargo did not work properly in its former location in
      islandora_scholar_embargo and it does not work properly here. Library
      personal report that PDF embargo might work, but other levels and
      durations of embargo are not used because they don't work properly. This
      is an area of functionality that will have to be fixed after OpenSky is
      hosted at Discovery Garden.
*/

/**
 * Recalculate the "Embargo Options" with opensky-specific values. We allow:
 * - 'object'
 * - 'PDF'
 * - 'OBJ'
 * - 'PROXY_MP3'
 */
function opensky_form_islandora_scholar_embargo_form_alter (&$form, &$form_state, $form_id) {
    //    dpm ('opensky_form_islandora_scholar_embargo_form_alter');

    $options = array(
        'object' => t('Object-level embargo'),
        'datastream' => t('Datastream'),
    );

    // add options for the object datastreams
    $pid = $form['pid']['#value'];
    $object = islandora_object_load($pid);
    foreach ($object as $datastream) {
        $options[$datastream->id] = $datastream->label;
    }

    // allowable options - these are the only "embargop options" allowed in opensky
    $allowed_options = array (
        'object' => '',
        'PDF' => '',    // OPenSky added PDF to original 
        'OBJ' => '',
        'PROXY_MP3' => '',
    );

    // reduce options to those allowable
    $options = array_intersect_key($options, $allowed_options);
    $form['dsid']['#options'] = $options;

    // stick opensky form submit handler in before islandora_scholar version
    module_load_include('inc', 'opensky', 'includes/utilities');
    //    array_splice ($form['#submit'], 0, 0, 'opensky_embargo_form_submit');

  
    $form['#submit'] = array('opensky_embargo_form_submit');

    // dpm ($form);

}

/**
 * We make enough opensky-specific changes to
 * islandora_scholar_embargo_form_submit that we have to copy the islandora_scholar 
 * function here and modify it, thus replacing it with opensky_embargo_form_submit
 */
function opensky_embargo_form_submit (array $form, array &$form_state) {
    //    dpm ('opensky_embargo_form_submit');
    $embargoed = !empty($form_state['islandora_embargo']);
    $pid = $form_state['values']['pid'];
    $object = islandora_object_load($pid);
    // We are lifting an embargo from an object that already has one.
    if (isset($form_state['values']['update_options']) && $form_state['values']['update_options'] === 'lift_embargo') {
        $dsids = array();

        // This will have to be re-defined if and when we decide to allow
        // multiple separate embargoing on an object likely by using this
        // function: islandora_scholar_embargo_lift_associated_except(). For now
        // we can only have one embargo.

        foreach ($form_state['islandora_embargo'] as $embargo_vals) {
            $type = $embargo_vals['type'];
            if ($type !== 'Object') {
                $dsids[] = $type;
            }
            // OpenSky
            islandora_scholar_embargo_lift_embargo($pid, $dsids);
            // If an embargo on audio datastream, also remove the embargo on the
            // OBJ datastream
            if ($dsids == 'PROXY_MP3') {
                islandora_scholar_embargo_lift_embargo($pid, 'OBJ');
            }
            drupal_set_message(t('The embargo on @label (@pid) has been lifted!', array(
                '@label' => $object->label,
                '@pid' => $pid,
            )));
            // end OpenSky
        }
        // Batch down and down and down.
        if (isset($form_state['values']['recurse']) && $form_state['values']['recurse']) {
            $option = $form_state['values']['recurse_options'];
            $query_array = $form_state['query_choices'][$option];
            $batch = array(
                'title' => t('Lifting Embargoes'),
                'progress_message' => t('Please wait if many objects are being updated this could take a few minutes.'),
                'operations' => array(
                    array(
                        'islandora_scholar_embargo_lift_batch_function',
                        array($pid, $dsids, $query_array),
                    ),
                ),
                'finished' => 'islandora_scholar_embargo_lift_batch_finished',
                'file' => drupal_get_path('module', 'islandora_scholar_embargo') . '/includes/batch.inc',
            );
            batch_set($batch);
        }
        islandora_scholar_embargo_lift_embargo($pid, $dsids);
        drupal_set_message(t('The embargo on @label (@pid) has been lifted!', array(
            '@label' => $object->label,
            '@pid' => $pid,
        )));
    }
    elseif (isset($form_state['values']['update_date_options'])) {
        // Embargoed indefinitely.
        if ($form_state['values']['update_date_options'] === 'indefinite_embargo') {
            $end = 'indefinite';
        }
        else {
            $time = $form_state['values']['embargo_date'];
            $date_string = $time['year'] . '-' . $time['month'] . '-' . $time['day'];
            $end = gmdate("Y-m-d\TH:i:s\Z", strtotime($date_string));
        }

        // Object is currently embargoed and we are updating things.
        if ($embargoed) {
            $dsids = array();
            foreach ($form_state['islandora_embargo'] as $embargo_vals) {
                $type = $embargo_vals['type'];
                if ($embargo_vals['type'] == 'Object') {
                    $dsids = NULL;
                }
                else {
                    $dsids[] = $type;
                }
            }
        }
        // New embargo entry.
        else {
            if ($form_state['values']['dsid'] === 'object') {
                $dsids = NULL;
            }
            else {
                // OpenSky
                //        $dsid_options =
                //        array_filter($form_state['values']['ds_options']);
                //        $other_options = array();
                //        if ($form_state['values']['ds_options_other']) {
                //          $other_options =
                //        drupal_map_assoc(array_values(array_filter(preg_split('/[,|\s]/',
                //        trim($form_state['values']['ds_other'])))));
                //        }
                //        $dsids = array_unique(array_merge($dsid_options, $other_options));
                $dsids = (array)$form_state['values']['dsid'];
                // end OpenSky
            }
        }
        // Currently we just overwrite what's already in place. In the future, for
        // multiple embargoes on a single object, this will have to be adjusted.

        // If we're recursing do those objects in a batch.
        if (isset($form_state['values']['recurse']) && $form_state['values']['recurse'] && $form_state['values']['recurse_options'] != 'newchildren') {
            $option = $form_state['values']['recurse_options'];
            $query_array = $form_state['query_choices'][$option];
            $batch = array(
                'title' => t('Updating Embargoes'),
                'progress_message' => t('Please wait if many objects are being updated this could take a few minutes.'),
                'operations' => array(
                    array(
                        'islandora_scholar_embargo_apply_batch_function',
                        array($pid, $dsids, $end, $query_array),
                    ),
                ),
                'finished' => 'islandora_scholar_embargo_apply_batch_finished',
                'file' => drupal_get_path('module', 'islandora_scholar_embargo') . '/includes/batch.inc',
            );
            batch_set($batch);
        }
        islandora_scholar_embargo_set_embargo($pid, $dsids, $end);
        islandora_scholar_embargo_set_display_message($pid, $dsids, $end);
    }
}
