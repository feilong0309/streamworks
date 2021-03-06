package gov.pnnl.datasciences.sparkstreaming;
import com.google.common.collect.Lists;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;
import org.apache.spark.SparkConf;
import org.apache.spark.SparkContext;
import org.apache.spark.api.java.JavaPairRDD;
import org.apache.spark.api.java.function.FlatMapFunction;
import org.apache.spark.api.java.function.Function;
import org.apache.spark.api.java.function.Function2;
import org.apache.spark.api.java.function.PairFunction;
import org.apache.spark.storage.StorageLevel;
import org.apache.spark.streaming.Duration;
import org.apache.spark.streaming.api.java.JavaDStream;
import org.apache.spark.streaming.api.java.JavaPairDStream;
import org.apache.spark.streaming.api.java.JavaReceiverInputDStream;
import org.apache.spark.streaming.api.java.JavaStreamingContext;
import org.apache.spark.streaming.receiver.Receiver;

import scala.Tuple2;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.io.UnsupportedEncodingException;
import java.net.ConnectException;
import java.net.Socket;
import java.util.regex.Pattern;

public class SparkCustomStreaming extends Receiver<String> {
	private static final Pattern SPACE = Pattern.compile(" ");

	public static void main(String[] args) throws FileNotFoundException, UnsupportedEncodingException {
    if (args.length < 2) {
      System.err.println("Usage: JavaCustomReceiver <hostname> <port>");
      System.exit(1);
    }

    // Create the context with a 1 second batch size
    SparkConf sparkConf = new SparkConf().setAppName("JavaCustomReceiver");
    JavaStreamingContext ssc = new JavaStreamingContext(sparkConf, new Duration(1000));

    
    // Create a input stream with the custom receiver on target ip:port and count the
    // words in input stream of \n delimited text (eg. generated by 'nc')
   
    JavaReceiverInputDStream<String> lines = ssc.receiverStream(
      new SparkCustomStreaming(args[0], Integer.parseInt(args[1])));
    
    
   
    JavaDStream<String> words = lines.flatMap(new FlatMapFunction<String, String>() {
      
      public Iterable<String> call(String x) {
        return Lists.newArrayList(x);
      }
    });
   

  
    
  	  
  	 JavaDStream<String> UDP_Flows = words.filter(
  			new Function<String, Boolean>() {
  			public Boolean call(String word) { return word.contains("UDP"); }
  			}
  			);
  	 JavaDStream<String> TCP_Flows = words.filter(
   			new Function<String, Boolean>() {
   			public Boolean call(String word) { return word.contains("TCP"); }
   			}
   			);
  	 JavaDStream<String> ICMP_Flows = words.filter(
    			new Function<String, Boolean>() {
    			public Boolean call(String word) { return word.contains("ICMP"); }
    			}
    			);
  	JavaDStream<String> FTP_Flows = words.filter(
			new Function<String, Boolean>() {
			public Boolean call(String word) { return word.contains("FTP"); }
			}
			);
  	 
  	 JavaPairDStream<String, Integer> UDP_FLOW_Counts = UDP_Flows.mapToPair(
  	  	      new PairFunction<String, String, Integer>() {
  	  	    	  
  	  	        public Tuple2<String, Integer> call(String s) {
  	  	        	System.out.println("INSIDE MAP: "+s);
  	  	        	String [] pieces = s.split(" +");
  	  	        	if(pieces.length>8){
  	  	          return new Tuple2<String, Integer>(pieces[4]+" - "+pieces[6], Integer.valueOf(pieces[8]));
  	  	        	}
  	  	        	else{
  	  	        		return new Tuple2<String, Integer>("incompleteBuffer", 1);
  	  	    	        
  	  	        	}
  	  	        }
  	  	      });
  	  	      
  	JavaPairDStream<String, Integer> UDP_FLOW_Counts_SmallTimeSlot= UDP_FLOW_Counts.reduceByKey(new Function2<Integer, Integer, Integer>() {
  	  	        
  	  	        public Integer call(Integer i1, Integer i2) {
  	  	        	System.out.println("INSIDE REDUCE: ");
  	  	          return i1 + i2;
  	  	        }
  	  	      });
  	  	      
//  	JavaPairDStream<String, Integer> UDP_FLOW_Counts_LargeTimeSlot = UDP_FLOW_Counts.reduceByKeyAndWindow(new Function2<Integer, Integer, Integer>() {
//	  	        
//	  	        public Integer call(Integer i1, Integer i2) {
//	  	        	System.out.println("INSIDE REDUCE: ");
//	  	          return i1 + i2;
//	  	        }
//	  	      }, new Duration(60000),new Duration(1000));

 // 	  	    UDP_FLOW_Counts.print();

  	
  	UDP_FLOW_Counts_SmallTimeSlot.foreach(
  			new Function<JavaPairRDD<String, Integer>, Void> () {
  			public Void call(JavaPairRDD<String, Integer> rdd) {
  			String out = "\nUDP Flows 1 sec integration:\n";
  			for (Tuple2<String, Integer> t: rdd.take(10)) {
  			out = out + t.toString() + "\n";
  			}
  			System.out.println(out);
  			return null;
  			}
  			}
  			);
//  	UDP_FLOW_Counts_LargeTimeSlot.foreach(
//  			new Function<JavaPairRDD<String, Integer>, Void> () {
//  			public Void call(JavaPairRDD<String, Integer> rdd) {
//  			String out = "\nUDP Flows 1 minute integration:\n";
//  			for (Tuple2<String, Integer> t: rdd.take(10)) {
//  			out = out + t.toString() + "\n";
//  			}
//  			System.out.println(out);
//  			return null;
//  			}
//  			}
//  			); 	
  	
  	  	 JavaPairDStream<String, Integer> TCP_FLOW_Counts = TCP_Flows.mapToPair(
  	  	  	      new PairFunction<String, String, Integer>() {
  	  	  	    	  
  	  	  	        public Tuple2<String, Integer> call(String s) {
  	  	  	        	System.out.println("INSIDE MAP: "+s);
  	  	  	        	String [] pieces = s.split(" +");
  	  	  	        	if(pieces.length>8){
  	  	  	        		
  	  	  	        		String key = pieces[4]+" - "+pieces[6];
  	  	  	        		String val = pieces[8];
  	  	  	        		if(pieces[9].equalsIgnoreCase("M")){
  	  	  	        			
  	  	  	        			float actualVal = Float.valueOf(val)*1000000;
  	  	  	        			val = String.valueOf((int)actualVal);
  	  	  	        			
  	  	  	        		}
  	  	  	        		else if(pieces[9].equalsIgnoreCase("G")){
  	  	  	        		float actualVal = Float.valueOf(val)*1000000000;
	  	  	        			val = String.valueOf(actualVal);
  	  	  	        		}
  	  	  	          return new Tuple2<String, Integer>(key, Integer.valueOf(val));
  	  	  	        	}
  	  	  	        	else{
  	  	  	        		return new Tuple2<String, Integer>("incompleteBuffer", 1);
  	  	  	    	        
  	  	  	        	}
  	  	  	        }
  	  	  	      }).reduceByKey(new Function2<Integer, Integer, Integer>() {
  	  	  	        
  	  	  	        public Integer call(Integer i1, Integer i2) {
  	  	  	        	System.out.println("INSIDE REDUCE: ");
  	  	  	          return i1 + i2;
  	  	  	        }
  	  	  	      });


  	  	
  	  	  	TCP_FLOW_Counts.foreach(
  	  			new Function<JavaPairRDD<String, Integer>, Void> () {
  	  			public Void call(JavaPairRDD<String, Integer> rdd) {
  	  			String out = "\nTCP Flows:\n";
  	  			for (Tuple2<String, Integer> t: rdd.take(10)) {
  	  			out = out + t.toString() + "\n";
  	  			}
  	  			System.out.println(out);
  	  			return null;
  	  			}
  	  			}
  	  			);
  	  	  	
  	   	 JavaPairDStream<String, Integer> ICMP_FLOW_Counts = ICMP_Flows.mapToPair(
  	  	  	      new PairFunction<String, String, Integer>() {
  	  	  	    	  
  	  	  	        public Tuple2<String, Integer> call(String s) {
  	  	  	        	System.out.println("INSIDE MAP: "+s);
  	  	  	        	String [] pieces = s.split(" +");
  	  	  	        	if(pieces.length>8){
  	  	  	          return new Tuple2<String, Integer>(pieces[4]+" - "+pieces[6], Integer.valueOf(pieces[8]));
  	  	  	        	}
  	  	  	        	else{
  	  	  	        		return new Tuple2<String, Integer>("incompleteBuffer", 1);
  	  	  	    	        
  	  	  	        	}
  	  	  	        }
  	  	  	      }).reduceByKey(new Function2<Integer, Integer, Integer>() {
  	  	  	        
  	  	  	        public Integer call(Integer i1, Integer i2) {
  	  	  	        	System.out.println("INSIDE REDUCE: ");
  	  	  	          return i1 + i2;
  	  	  	        }
  	  	  	      });


  	  	
  	  	  	ICMP_FLOW_Counts.foreach(
  	  			new Function<JavaPairRDD<String, Integer>, Void> () {
  	  			public Void call(JavaPairRDD<String, Integer> rdd) {
  	  			String out = "\nICMP Flows:\n";
  	  			for (Tuple2<String, Integer> t: rdd.take(10)) {
  	  			out = out + t.toString() + "\n";
  	  			}
  	  			System.out.println(out);
  	  			return null;
  	  			}
  	  			}
  	  			);
  	  	  	
  	  	PrintWriter writer = new PrintWriter("runtimes.txt", "UTF-8");
  	  	long startTime = System.currentTimeMillis();
  	    
  	    System.out.println("Start time of the experiment: "+ startTime);
  	    writer.write("StartTime: "+startTime+"\n");
  	    writer.flush();
    ssc.start();
   
    ssc.awaitTermination();
    
    long endTime = System.currentTimeMillis();
    writer.write("endTime: "+endTime+"\n");
	    System.out.println("End time of the experiment: "+ endTime);
	    writer.write("totalTime: "+(endTime-startTime)+"\n");
	    writer.close();
	    System.out.println("Total time taken in milli secs: "+ (endTime-startTime));
    
  }

	// ============= Receiver code that receives data over a socket
	// ==============

	String host = null;
	int port = -1;

	public SparkCustomStreaming(String host_, int port_) {
		super(StorageLevel.MEMORY_AND_DISK_2());
		host = host_;
		port = port_;
	}

	public void onStart() {
		// Start the thread that receives data over a connection
		new Thread() {
			@Override
			public void run() {
				receive();
			}
		}.start();
	}

	public void onStop() {
		// There is nothing much to do as the thread calling receive()
		// is designed to stop by itself isStopped() returns false
	}

	/** Create a socket connection and receive data until receiver is stopped */
	private void receive() {
		Socket socket = null;
		String userInput = null;

		try {
			// connect to the server
			socket = new Socket(host, port);
			Path pt = new Path(
					"file:///home/hduser/netflow-sample.txt");

			Configuration conf = new Configuration();
			FileSystem fs = pt.getFileSystem(conf);
			
			BufferedReader in = new BufferedReader(new InputStreamReader(
					fs.open(pt)));

			// Until stopped or connection broken continue reading
			while (!isStopped() && (userInput = in.readLine()) != null) {
				System.out.println("Received data '" + userInput + "'");
				store(userInput);
			}
			in.close();
			socket.close();

			// Restart in an attempt to connect again when server is active
			// again
			restart("Trying to connect again");
		} catch (ConnectException ce) {
			// restart if could not connect to server
			restart("Could not connect", ce);
		} catch (Throwable t) {
			restart("Error receiving data", t);
		}
	}
}
