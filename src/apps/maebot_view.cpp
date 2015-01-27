#include "maebot_view.h"

#include <string>

// Variable definition for states
vx_state_t vx_state;
State state;
Odo_state odo_state;
IMU_State imu_state;
Occupancy_Grid_State occupancy_grid_state;

/***********
   this is not an actual rotation matrix
   effectively flips point about x axis
   to be flipped about y axis after (see line 176)
 *********/
void rotate_matrix_z(float* x, float* y, float theta) {
	float new_x, new_y;
	new_x = *x * cosf(theta) + *y * sinf(theta);
	new_y = (-1) * (*x) * sinf(theta) + *y * cosf(theta);
	*x = new_x;
	*y = new_y;
}

void rplidar_feedback_handler(const lcm_recv_buf_t *rbuf, const char *channel, const maebot_laser_scan_t *scan, void *user)
{
	printf("Handling rplidar\n");

	//ADD_OBJECT(vxo_line, (vxo_mesh_style(vx_green)));
	int i, npoints;
	float single_line[6]; // x1, y1, z1, x2, y2, z2
	const float* colors[4] = {vx_blue, vx_purple, vx_orange, vx_yellow};

	npoints = 2;
	single_line[0] = /*maebot starting x*/ (matd_get(state.bot, 0, 0));
	single_line[1] = /*maebot starting y*/ (matd_get(state.bot, 1, 0));
	single_line[2] = /*maebot starting z*/ 0.0;

	char rp_buffer[32];
	sprintf(rp_buffer, "rp%d", 0);

	vx_buffer_t *mybuf = vx_world_get_buffer(vx_state.world, rp_buffer);
	//printf("\t%f\t%f\n", matd_get(state.bot, 0, 0), matd_get(state.bot, 1, 0));


	for(i = 0; i < scan->num_ranges; ++i){
		// currently centered around origin, will need to be centered around maebot position
		if(scan->intensities[i] <= 0)
			continue;
		float x, y;
		x = (scan->ranges[i]) * cosf(scan->thetas[i]);
		y = (scan->ranges[i]) * sinf(scan->thetas[i]);
		rotate_matrix_z(&x, &y, matd_get(state.bot, 2, 0));
		single_line[3] = single_line[0] + x;
		single_line[4] = single_line[1] - y;
		single_line[5] = 0.0;
		
		vx_resc_t *verts = vx_resc_copyf(single_line, npoints*3);
		vx_object_t *line = vxo_lines(verts, npoints, GL_LINES, 
			vxo_points_style(colors[state.rp_counter % 4], 2.0f));
		vx_buffer_add_back(mybuf, line);
	}
	vx_buffer_swap(mybuf);
}

void motor_feedback_handler (const lcm_recv_buf_t *rbuf, const char *channel, const maebot_motor_feedback_t *msg, void *user)
{
	// int res = system ("clear");
	// if (res)
	// 	printf ("system clear failed\n");
	// printf("Handling motor\n");
	
	//update state
	if(!odo_state.init){
		odo_state.left = msg->encoder_left_ticks;
		odo_state.right = msg->encoder_right_ticks;
		odo_state.init = 1;
	} else{
		odo_state.delta_left = msg->encoder_left_ticks - odo_state.left;
		odo_state.delta_right = msg->encoder_right_ticks - odo_state.right;
		//printf("\t%d\t%d\n", delta_left, delta_right);
		odo_state.delta_s_l = (DISTANCE_TICK * (float)odo_state.delta_left);
		odo_state.delta_s_r = (DISTANCE_TICK * (float)odo_state.delta_right);
		odo_state.delta_s =((float)(odo_state.delta_s_l + odo_state.delta_s_r))/2.0;
		odo_state.delta_theta = ((float)(odo_state.delta_s_r - odo_state.delta_s_l))/WHEEL_BASE + matd_get(state.bot, 2, 0);
		//printf("\t%f\t%f\t%f\n", delta_s_l, delta_s_r, delta_theta);
		matd_put(state.bot, 2, 0, odo_state.delta_theta);
		
		if(odo_state.delta_s < 0)
			odo_state.delta_s = odo_state.delta_s*(-1.0);
		
		odo_state.delta_x = odo_state.delta_s*fcos((float)odo_state.delta_theta) + matd_get(state.bot, 0, 0);
		matd_put(state.bot, 0, 0, odo_state.delta_x);
		
		odo_state.delta_y = odo_state.delta_s*fsin((float)odo_state.delta_theta) + matd_get(state.bot, 1, 0);
		matd_put(state.bot, 1, 0, odo_state.delta_y);
		
		//printf("%f\t%f\n", delta_x, delta_y);
		
		odo_state.left = msg->encoder_left_ticks;
		odo_state.right = msg->encoder_right_ticks;
		
		
		//printf("%f\t%f\t%f\n", matd_get(state.bot, 0, 0), matd_get(state.bot, 1, 0), matd_get(state.bot, 2, 0));
		char odo_buffer[32];
		float pt[3] = {(float)matd_get(state.bot, 0, 0), (float)matd_get(state.bot, 1, 0), 0.0};
		sprintf(odo_buffer, "odo%d", state.odo_counter++);
		vx_resc_t *one_point = vx_resc_copyf(pt,3);
		vx_buffer_t *buf = vx_world_get_buffer(vx_state.world, odo_buffer);
		vx_object_t *trace = vxo_points(one_point, 1, vxo_points_style(vx_red, 2.0f)); 
		                    /*vxo_chain(vxo_mat_translate3(matd_get(state.bot, 0, 0), matd_get(state.bot, 1, 0), 0.0),
				       vxo_points(one_point, 1, vxo_points_style(vx_red, 2.0f)));*/
		vx_buffer_add_back(buf, trace);
		vx_buffer_swap(buf);
	}//end else
	
	//usleep(100000);
}

void sensor_data_handler (const lcm_recv_buf_t *rbuf, const char *channel, const maebot_sensor_data_t *msg, void *user)
{
  /*
  int res = system ("clear");
  if (res)
    printf ("system clear failed\n");
  
  printf ("Subscribed to channel: MAEBOT_SENSOR_DATA\n");
  printf ("utime: %"PRId64"\n", msg->utime);
  printf ("accel[0, 1, 2]:        %d,\t%d,\t%d\n",
	  msg->accel[0], msg->accel[1], msg->accel[2]);
  printf ("gyro[0, 1, 2]:         %d,\t%d,\t%d\n",
	  msg->gyro[0], msg->gyro[1], msg->gyro[2]);
  printf ("gyro_int[0, 1, 2]:     %"PRId64",\t%"PRId64",\t%"PRId64"\n",
	  msg->gyro_int[0], msg->gyro_int[1], msg->gyro_int[2]);
  printf ("line_sensors[0, 1, 2]: %d,\t%d,\t%d\n",
	  msg->line_sensors[0], msg->line_sensors[1], msg->line_sensors[2]);
  printf ("range: %d\n", msg->range);
  printf ("user_button_pressed: %s\n", msg->user_button_pressed ? "true" : "false");
  printf ("power_button_pressed: %s\n", msg->power_button_pressed ? "true" : "false");
  */
  //update state
  //x and y first
  if(imu_state.prev_time == 0.0){
  	imu_state.prev_time = msg->utime;	
  }else{
  	imu_state.delta_time = msg->utime - imu_state.prev_time;
  	imu_state.delta_time = imu_state.delta_time / 1000000.0;
  	imu_state.delta_time_squared = powf(imu_state.delta_time, 2.0);
  	imu_state.prev_time = msg->utime;

  	imu_state.a_x = (float)msg->accel[0] * ACCEL_2_MET;
  	imu_state.a_y = (float)msg->accel[1] * ACCEL_2_MET;
  	imu_state.v_theta = (float)msg->gyro[2] * GYRO_2_RADS;

    //update x and y in state
  	imu_state.delta_x_local = matd_get(imu_state.bot, 0, 1) * imu_state.delta_time + 0.5 * imu_state.a_x * imu_state.delta_time_squared;
  	imu_state.delta_y_local = matd_get(imu_state.bot, 1, 1) * imu_state.delta_time + 0.5 * imu_state.a_y * imu_state.delta_time_squared;

  	imu_state.delta_x_global = imu_state.delta_x_local * cosf(matd_get(imu_state.bot, 2, 0)) +
  	imu_state.delta_y_local * cosf((M_PI/2) - matd_get(imu_state.bot, 2, 0));
  	imu_state.delta_y_global = imu_state.delta_x_local * sinf(matd_get(imu_state.bot, 2, 0)) +
  	imu_state.delta_y_local * sinf((M_PI/2) - matd_get(imu_state.bot, 2, 0));
    //update x, y state
  	matd_put(imu_state.bot, 0, 0, matd_get(imu_state.bot, 0, 0) + imu_state.delta_x_global);
  	matd_put(imu_state.bot, 1, 0, matd_get(imu_state.bot, 1, 0) + imu_state.delta_y_global);

    //get instantaneous velocity
    //imu_state.v_x_i = (float)msg->accel[0] * ACCEL_2_MET * imu_state.delta_time;
    //imu_state.v_y_i = (float)msg->accel[1] * ACCEL_2_MET * imu_state.delta_time;

    //update Vx, Vy state
  	matd_put(imu_state.bot, 0, 1, matd_get(imu_state.bot, 0, 1) + imu_state.a_x * imu_state.delta_time);
  	matd_put(imu_state.bot, 1, 1, matd_get(imu_state.bot, 1, 1) + imu_state.a_y * imu_state.delta_time);


    //update theta
  	imu_state.delta_theta = imu_state.v_theta * imu_state.delta_time;

  	matd_put(imu_state.bot, 2, 0, matd_get(imu_state.bot, 2, 0) + imu_state.delta_theta);

    //update Vtheta
    // imu_state.v_theta_i = imu_state.a_theta * imu_state.delta_time;
    //matd_put(imu_state.bot, 2, 2, matd_get(imu_state.bot, 2, 2) + imu_state.v_theta_i);

  }

  //printf("%f\t\t%f\t\t%f\n", matd_get(state.bot, 0, 0), matd_get(state.bot, 1, 0), matd_get(state.bot, 2, 0));
  //printf("%f\t%f\t\t%f\n", matd_get(imu_state.bot, 0, 0), matd_get(imu_state.bot, 1, 0), matd_get(imu_state.bot, 2, 0));
  
  char imu_buffer[32];
  float pt[3] = {(float)((-0.125) * (float)matd_get(imu_state.bot, 0, 0)), (float)((-0.125) * matd_get(imu_state.bot, 1, 0)), 0.0};
  sprintf(imu_buffer, "imu%d", state.imu_counter++);
  //printf("\t\t%s\n", state.buffer);
  vx_resc_t *one_point = vx_resc_copyf(pt,3);
  vx_buffer_t *imu_buf = vx_world_get_buffer(vx_state.world, imu_buffer);
  vx_object_t *imu_trace = vxo_points(one_point, 1, vxo_points_style(vx_green, 2.0f)); 
  vx_buffer_add_back(imu_buf, imu_trace);
  vx_buffer_swap(imu_buf);
  
}

void occupancy_grid_handler(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const maebot_occupancy_grid_t* msg, void* state) 
{
    occupancy_grid_state.grid->fromLCM(*msg);
    
    int w = occupancy_grid_state.grid->widthInCells();
    int h = occupancy_grid_state.grid->heightInCells();

    std::cout << "h " << h << ", w " << w << "\n";

    image_u8_t *im = image_u8_create (w, h);
    for (int i = 0; i < w; ++i) {
        for (int j = 0; j < h; ++j) {
            im->buf[j*w+i] = 127 - occupancy_grid_state.grid->logOdds(i,j);
            std::cout << "buf[" << j*w+i << "]: " << occupancy_grid_state.grid->logOdds(i,j) << std::endl;
        }
    }
    std::cout << "done shifting\n";

    if (im != NULL) {

        vx_object_t * vo = vxo_image_from_u8(im, VXO_IMAGE_FLIPY, VX_TEX_MIN_FILTER);

        vx_buffer_t *vb = vx_world_get_buffer(vx_state.world, "map");
        vx_buffer_add_back(vb, vxo_pix_coords(VX_ORIGIN_TOP_LEFT,
                                              vxo_chain (vxo_mat_translate3 (0, 0, -0.001),
                                                         vo)));
        vx_buffer_swap(vb);
    }
    else {
        printf("Error converting to image");
    }

    image_u8_destroy(im);
}


void* lcm_lidar_handler(void *args)
{
	State *lcm_state = (State*) args;
	while(1){
		lcm_handle (lcm_state->lidar_lcm);
	}
	return NULL;
}

void* lcm_motor_handler(void *args)
{
	State *lcm_state = (State*) args;
	while(1){
		lcm_handle(lcm_state->motor_lcm);
	}
	return NULL;
}

void* lcm_imu_handler(void *args)
{
	State *lcm_state = (State*) args;
	while(1){
		lcm_handle(lcm_state->imu_lcm);
	}
	return NULL;
}

void* lcm_occupancy_grid_handler(void *args)
{
    State *lcm_state = (State*) args;
    //int update_hz = 30;

    while(1){
        lcm_state->lcm.handle();

    }
    return NULL;
}


void display_finished(vx_application_t * app, vx_display_t * disp)
{
	// XXX layer leak
}

void display_started(vx_application_t * app, vx_display_t * disp)
{
	vx_state_t * VX_state = (vx_state_t*)app->impl;
	
	vx_layer_t * layer = vx_layer_create(VX_state->world);
	vx_layer_set_display(layer, disp);
	// vx_layer_set_background_color(layer, vx_black);	

	// XXX bug in world
	draw(VX_state->world, VX_state->obj_data);
}


void draw(vx_world_t * world, zarray_t * obj_data)
{
	vx_buffer_t * vb = vx_world_get_buffer(world, "zoo");
	
	int cols = sqrt(zarray_size(obj_data)) + 1;
	int rows = zarray_size(obj_data)/cols + 1;
	int grid = 4;
	
	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < cols; x++) {
			int idx = y*cols + x;
			if (idx >= zarray_size(obj_data))
				break;
			
			obj_data_t  data;
			zarray_get(obj_data, idx, &data);
			vx_object_t * vo = data.obj;
			
			vx_buffer_add_back(vb, vxo_chain(vxo_mat_translate2(x*grid + grid/2, rows*grid - (y*grid +grid/2)),
				vxo_chain(vxo_mat_scale(grid),
					vxo_rect(vxo_lines_style(vx_gray, 2))),
				vo));
			// XXX Text, box outlines
		}
	}
	vx_buffer_swap(vb); // XXX Could name buffers by object name
}


int Maebot_View::start (int argc, char** argv)
{
	state.odo_counter = 0;
	state.rp_counter = 0;
	state.imu_counter = 0;
	fasttrig_init();
	vx_global_init();
	
	vx_state.world = vx_world_create();
	vx_state.obj_data = zarray_create(sizeof(obj_data_t));
	// ADD_OBJECT(obj_type, args);
	
	vx_application_t app;
	app.impl=&vx_state;
	app.display_started = display_started;
	app.display_finished = display_finished;
	
	gdk_threads_init();
	gdk_threads_enter();
	gtk_init(&argc, &argv);
	
	state.bot = matd_create(3,1); // [x][y][theta]
	imu_state.bot = matd_create(3,2);
	imu_state.prev_time = 0.0;
	
	state.motor_lcm = lcm_create (NULL);
	if(!state.motor_lcm)
		return 1;
	
	state.lidar_lcm = lcm_create(NULL);
	if(!state.lidar_lcm)
		return 1;

	state.imu_lcm = lcm_create(NULL);
	if(!state.imu_lcm)
		return 1;
	
	maebot_motor_feedback_t_subscribe (state.motor_lcm,
		"MAEBOT_MOTOR_FEEDBACK",
		motor_feedback_handler,
		NULL);
	
	maebot_laser_scan_t_subscribe (state.lidar_lcm,
		"MAEBOT_LASER_SCAN",
		rplidar_feedback_handler,
		NULL);

	maebot_sensor_data_t_subscribe (state.imu_lcm,
		"MAEBOT_SENSOR_DATA",
		sensor_data_handler,
		NULL);



    state.lcm.subscribeFunction("OCCUPANCY_GRID", occupancy_grid_handler, (void*) NULL);
    
	pthread_t lcm_lidar_thread, lcm_motor_thread, lcm_imu_thread, lcm_occupancy_grid_thread;
	pthread_create(&lcm_motor_thread, NULL, lcm_motor_handler, (void*)(&state));
	pthread_create(&lcm_lidar_thread, NULL, lcm_lidar_handler, (void*)(&state));
	pthread_create(&lcm_imu_thread, NULL, lcm_imu_handler, (void*)(&state));
	pthread_create(&lcm_occupancy_grid_thread, NULL, lcm_occupancy_grid_handler, (void*)(&state));

	vx_gtk_display_source_t * appwrap = vx_gtk_display_source_create(&app);
	GtkWidget * window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	GtkWidget * canvas = vx_gtk_display_source_get_widget(appwrap);
	gtk_window_set_default_size (GTK_WINDOW (window), 1280, 720);
	gtk_container_add(GTK_CONTAINER(window), canvas);
	gtk_widget_show (window);
	gtk_widget_show (canvas); // XXX Show all causes errors!
	
	g_signal_connect_swapped(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
	

	
	gtk_main (); // Blocks as long as GTK window is open
	gdk_threads_leave ();
	
	vx_gtk_display_source_destroy(appwrap);
	vx_global_destroy();

	return 0;
}





/* #include <lcm/lcm-cpp.hpp>
 *
 * class State {
 * public:
 *   lcm::LCM lcm;
 *   int usefulVariable;
 * };
 *
 * void onMessage(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const MessageType* msg, State* state) {
 *   // do something with the message.
 * }
 *
 * int main(int argc, char** argv) {
 *   State* state = new State;
 *   state->lcm.subscribe("CHANNEL", onMessage, state);
 *   while(true)
 *     state->lcm.handle();
 *   delete state;
 *   return 0;
 * }

 * #include <lcm/lcm-cpp.hpp>
 *
 * void onMessage(const lcm::ReceiveBuffer* rbuf, const std::string& channel, void*) {
 *   // do something with the message.  Raw message bytes are
 *   // accessible via rbuf->data
 * }
 *
 * int main(int argc, char** argv) {
 *   LCM::lcm lcm;
 *   lcm.subscribe("CHANNEL", onMessage, NULL);
 *   while(true)
 *     lcm.handle();
 *   return 0;
 * }

  inline int publish(const std::string& channel, const void *data,unsigned int datalen);
    inline int publish(const std::string& channel, const MessageType* msg);

 */