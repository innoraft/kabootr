<?php 
/**
 * @file
 * Create newsletter category with subscriptions
 *
 * @ingroup simplenews
 */
/**
 * Create user filters given in the account setting form 
 */
function simplenews_user_list_create_user_filters($newsletter_tid = NULL) {
	$widget_type = array();
	$widget_label = array();
	$widget_allowed_values = array();
	$widget_values = array();
	$user_field_ids = array();
	$widget_default = array();
	$widgets = array();
	
	// Give user field label, type and it's ids
	$user_field_query = db_select('field_config_instance','fci')
	->fields('fci',array('field_id','data'))
	->condition('entity_type','user')
	->execute()->fetchAll();
	foreach ($user_field_query as $value) {
		$user_field_ids[] = $value->field_id;
		$value_arr = unserialize($value->data);
		$widget_type[$value->field_id] = $value_arr['widget']['type'];
		$widget_label[$value->field_id] = $value_arr['label'];
	}
	
	// Give user fields allowed values sources
	$get_user_field_detail_query = db_select('field_config','fc')
	->fields('fc',array('id','data'))
	->condition('id',$user_field_ids)
	->execute()->fetchAll();
	foreach ($get_user_field_detail_query as $value) {
		$value_arr = unserialize($value->data);
		if (isset($value_arr['settings']['allowed_values'])) {
			$widget_allowed_values[$value->id] = $value_arr['settings']['allowed_values'];
		}
	}

	// Set user fields allowed values to widget according to field ids
	foreach ($widget_allowed_values as $w_key => $w_value) {
		foreach ($w_value as $key => $value) {
			
			// If value source is taxonomy term
			if (is_array($value)) {
				if (isset($value['vocabulary'])) {
					$vocab = taxonomy_vocabulary_machine_name_load($value['vocabulary']);
					$terms = taxonomy_get_tree($vocab->vid);
					foreach ($terms as $term) {
						$widget_values[$w_key]['tid_' . $term->tid] = $term->name;
					}
				}
			}
			
			// If value source is default in List type widget like Select List and Checkboxes 
			else {
				$widget_values[$w_key][$key] = $value;
			}
		}
	}
		
		// Retrieve already stored filters in simplenews_user_list
	if ($newsletter_tid != NULL) {
		$users_filters_query = db_select ( 'simplenews_user_list', 'sul' )
		->fields ( 'sul', array ('user_filters' ) )
		->condition ( 'tid', $newsletter_tid )
		->execute ()->fetchCol ( 0 );
		if (count($users_filters_query)!= 0) {
			foreach ($users_filters_query as $value)
				$users_filters = unserialize($value);
		foreach ( $users_filters as $w_key => $w_value ) {
			if (is_array ( $w_value )) {
				$widget_default [$w_key] = $w_value;
			} else {
				if (strpos ( $w_value, 'tid_' ) !== false) {
					$widget_default [$w_key] = 'tid_' . explode ( '_', $w_value )[1];
				} else {
					$widget_default [$w_key] = $w_value;
				}
			}
		}
		}
	}
// Create form elemtents according to user fields
	foreach ( $widget_type as $key => $value ) {
		switch ($value) {
			
			// Create Drop down menu
			case 'options_select' :
				$widget_values [$key][0] = "None";
				ksort($widget_values[$key]);
				if (!isset($widget_default[$key])) {
					$widget_default[$key] = array(0);
				}
				$widgets [$key] = array (
						'#type' => 'select',
						'#title' => t ( $widget_label [$key] ),
						'#options' => $widget_values [$key],
						'#default_value' => array($widget_default[$key]), 
				);
				break;
			
			// Create Checkboxes
			case 'options_buttons' :
				if (!isset($widget_default[$key])) {
					$widget_default[$key] = array(0);
				}
				$widgets [$key] = array (
						'#type' => 'checkboxes',
						'#title' => t ( $widget_label [$key] ),
						'#options' => $widget_values [$key],
						'#default_value' => $widget_default[$key],
				);
				break;
			
			// Create textfield
			default :
				if (!isset($widget_default[$key])) {
					$widget_default[$key] = '';
				}
				$widgets [$key] = array (
						'#type' => 'textfield',
						'#title' => t ( $widget_label [$key] ),
						'#size' => 30,
						'#default_value' => $widget_default[$key],
				);
		}
	}
	return $widgets;
}

/**
 * Submit handler, called after creating newsletter category. It creates the user
 * subcription list for created newsletter category
 */
function simplenews_user_list_admin_category_form_submit($form, &$form_state) {
	$users = '';

	// Retreive user filter from form state array
	$users_filters = array();
	foreach ( $form_state ['values'] as $w_key => $w_value ) {
		if (strpos ( $w_key, 'kabootr_user_filter_' ) !== false) {
			if (is_array($w_value)) {
				$selected_options = array_filter($w_value);
				if (count($selected_options) != 0) {
					$user_field_id = explode('_', $w_key)[3];
					$users_filters[$user_field_id] = $selected_options;
				}
			}
			elseif (!($w_value == '' || $w_value == '0')) {
				$user_field_id = explode('_', $w_key)[3];
				$users_filters[$user_field_id] = $w_value;
			}
		}
	}
	
	// Save or delete the status user filters and their value into simple_user_list table
	simplenews_user_list_create_list($users_filters, $form_state['values']['tid']);
	
	// If user filter is selected then get the user details according to filters
	if (count($users_filters) != 0) {
		// Get user field name and theid field ids
		$user_field_ids = array_keys($users_filters);
		$get_user_field_detail_query = db_select('field_config','fc')
		->fields('fc',array('id','field_name'))
		->condition('id',$user_field_ids)
		->execute()->fetchAll();
		
		// Get user details form applied filters
		$get_users_ids_query = new EntityFieldQuery();
		$get_users_ids_query->entityCondition('entity_type', 'user');
		foreach ($get_user_field_detail_query as $value) {
			// Prepare entity field query according to applied user filters
			if (is_array($users_filters[$value->id])) {
				$operator = "IN";
				$value_type = 'value';
				$value_filter = $users_filters[$value->id];
			}
			else {
				if (strpos($users_filters[$value->id],'tid_')!== false) {
					$value_type = 'tid';
					$value_filter = explode('_',$users_filters[$value->id])[1];
					$operator = "=";
				}
				else {
					$value_type = 'value';
					$operator = "=";
					$value_filter = $users_filters[$value->id];
				}
			}
			$get_users_ids_query->fieldCondition($value->field_name, $value_type, $value_filter, $operator);
		}
		$result = $get_users_ids_query->execute();
		$subscriber_uids = '';
		if (isset($result['user'])) {
			$uids = array_keys($result['user']);
			foreach ($uids as $uid) {
				if (simplenews_subscriber_load_by_uid($uid)) {
					$subscriber_uids[] = $uid;
				}
				else {
					$new_uids[] = $uid;
				}
			}
			if (isset($new_uids)) {
				$users = entity_load('user', $new_uids);
			}
			
			// Inactivate the old subscribers, and activate new subscribers come from new user filers
			simplenews_user_list_update_subcription_list($form_state['values']['tid'], $subscriber_uids);
		}
	}
	// If no filter is selected then subsribe the all user to that newsletter by default.
	else {
		$users = entity_load('user', FALSE);
	}
	// Subsribe users to newsletter
	if (!($users == '' || count($users) == 0) )
	{
		
		global $language;
		unset($users[0]);
		foreach ( $users as $value ) {
			$email = $value->mail;
			simplenews_subscribe_user ( $email, $form_state ['values'] ['tid'], FALSE, 'User List Subscription', $language );
			
			// If no user filter is selected, then activate all the already subscribers
			simplenews_user_list_update_subcription_list($form_state['values']['tid']);
		}
	}
}	

/**
 * Create or Update user list
 */
function simplenews_user_list_create_list($users_filters,$newsletter_tid) {
	if (count($users_filters) != 0) {
		
		// Check if user list is already exist to given newsletter category
		$users_filters_query = db_select ( 'simplenews_user_list', 'sul' )
		->fields ( 'sul', array ('sul_id' ) )
		->condition ( 'tid', $newsletter_tid )
		->execute ()->fetchCol ( 0 );
		
		// If there exist already user list, it update the filters
		if (count($users_filters_query)!= 0) {
			db_update('simplenews_user_list')
			->fields(array(
					'tid' => $newsletter_tid,
					'user_filters' => serialize($users_filters),
					'created' => REQUEST_TIME,
			))
			->condition ( 'tid', $newsletter_tid )
			->execute();
		}
		
		// If user list is not exist, it creates new one
		else {
			db_insert('simplenews_user_list')
			->fields(array(
			'tid' => $newsletter_tid,
			'user_filters' => serialize($users_filters),
			'created' => REQUEST_TIME,
			))
			->execute();
		}		
	}
	 
	// If no filter is selected, then delete user list
	else {
		db_delete('simplenews_user_list')
		->condition('tid',$newsletter_tid)
		->execute();
	}
}	

/**
 * Update newsletter subcription list
 * 
*/
function simplenews_user_list_update_subcription_list($newsletter_tid = NULL,$subscriber_uids = '') {
	
	// Get all newsletter subscribers of given newsletter category 
	$get_snid_query = db_select('simplenews_subscription','sn');
	$get_snid_query->innerJoin('simplenews_subscriber','ss', 'sn.snid = ss.snid ');
	$get_snid_query->fields('ss',array('snid','uid'))
	->condition('sn.tid',$newsletter_tid);
	$result = $get_snid_query->execute()->fetchAll();
	
	// Inactivate the old subscribers, and activate new subscribers come from new user filers
	if (is_array($subscriber_uids)) {
		$inactivate_snids = '';
		$activate_snids = '';
		foreach ($result as $value) {
			if (!in_array($value->uid, $subscriber_uids)) {
				$inactivate_snids[] = $value->snid;
			}
			else {
				$activate_snids[] = $value->snid;
			}
		}
		if (is_array($inactivate_snids)) {
			simplenews_subscription_activate($inactivate_snids, 0)	;
		}
		if (is_array($activate_snids)) {
			simplenews_subscription_activate($activate_snids, 1)	;
		}
	}
	
	// If no user filter is selected, then activate all the already subscribers
	else {
		foreach ($result as $value) {
				$activate_snids[] = $value->snid;
		}
		if (is_array($activate_snids)) {
			simplenews_subscription_activate($activate_snids, 1)	;
		}
	}
}	