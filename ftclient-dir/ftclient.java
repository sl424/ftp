/* code taken from 
 * https://docs.oracle.com/javase/tutorial/essential/io/file.html
 * Computer Networking by Kurose
 */

import java.io.*;
import java.net.*;
import static java.nio.file.StandardOpenOption.*;
import java.nio.file.*;

class ftclient {
	public static void main(String argv[])throws Exception  {
		 /* command line arguments */
		switch(argv.length){
			case 4:
				if (argv[2].equals("-l") && argv[3].matches("\\d+"))
					break;
			case 5:
				if (argv[2].equals("-g") && argv[4].matches("\\d+"))
					break;
			default:
				System.out.println(
						"Usage: java ftclient [host] [port] [-l | -g filename] [port]");
				System.exit(0);
		}

		/*
		System.out.print("port: ");
		System.out.println(argv[1]);
		System.out.print("data port: ");
		System.out.println(argv[argv.length-1]);
		*/

        int port = Integer.valueOf(argv[1]);
		int dataPort = Integer.valueOf(argv[argv.length-1]);
		String cmdline="";
		if (argv.length == 4) {
			cmdline = argv[3] + " " + argv[2];
			try{
				run_client( argv[0], argv[1], cmdline, argv[3], null);
			} catch(Exception e){
				System.out.println("error: sending list request");
			}
		} else if (argv.length == 5) {
			cmdline = argv[4] + " "+ argv[2] + " " + argv[3];
			try{
				run_client( argv[0], argv[1], cmdline, argv[4], argv[3]);
			} catch(Exception e){
				System.out.println("error: sending file request");
			}
		}
		//System.out.println(cmdline);
	}

	public static void run_client(String host, String port, 
								  String cmdline, String dataport, String file) 
			throws Exception {
		String status = "";
		String errorline = "";
		String dataline = "";

		/* setup control tcp connection */
		Socket controlSocket = new Socket(host, Integer.valueOf(port));
		String hostName = controlSocket.getInetAddress().getHostName();

		DataOutputStream outToServer = new DataOutputStream(controlSocket.getOutputStream());
		BufferedReader inFromServer =
			new BufferedReader(new InputStreamReader(controlSocket.getInputStream()));
		 PrintWriter out =
        new PrintWriter(controlSocket.getOutputStream(), true);

		/*send command to server  */
		//outToServer.writeBytes(cmdline);
		out.println(cmdline);

		status = inFromServer.readLine();
		//System.out.println(status);
		//System.out.println(dataport);

		/* handle response */
		if (status.contains("list data")){
			try{
				readDirectory(dataport,"");
			} catch (Exception e){
				System.out.println(e);
			}
		} else if (status.contains("get data")){
			try{
				writeFile(dataport,file );
			} catch (Exception e){
				System.out.println(e);
			}
			System.out.println(errorline);
		} else if (status.contains("get error")){
			try{
				errorline = inFromServer.readLine();
				System.out.println(hostName + ": "+errorline);
			} catch (Exception e){
				System.out.println(e);
			}
		}
		controlSocket.close();
	}

	public static void writeFile(String dataport, String filename) throws Exception{
		String dataline = "";
		int len = 1024;
		char[] cbuf = new char[len];
		/* setup data tcp connection */
		ServerSocket welcomeSocket = new ServerSocket(Integer.valueOf(dataport));
		//System.out.println("ready for data");
		Socket dataSocket = welcomeSocket.accept();
		String hostName = dataSocket.getInetAddress().getHostName();
		System.out.println("Receiving " + filename + " from " + 
				hostName+":"+dataport);

		BufferedReader dataFromServer =
			new BufferedReader(new InputStreamReader(dataSocket.getInputStream()));
		DataOutputStream dataToServer = new DataOutputStream(dataSocket.getOutputStream());

		Path file = Paths.get("./"+filename);
		try {
			// Create the empty file with default permissions, etc.
			Files.createFile(file);

			dataToServer.flush();
			dataToServer.writeBytes(">>");

			dataFromServer.read(cbuf, 0, len);
			dataline = new String(cbuf);

			while (!dataline.contains("@@")){
				/*append to file */
				byte[] bytebuf = dataline.getBytes();
				Files.write(file, bytebuf, APPEND);
				/* return confirmation */
				dataToServer.writeBytes(">>");
				dataFromServer.read(cbuf, 0, len);
				dataline = new String(cbuf);
			}

			/* write the last bytes */
			dataline = dataline.split("@@")[0];
			byte[] bytebuf = dataline.getBytes();
			Files.write(file, bytebuf, APPEND);
			System.out.println("File transfer complete");

		} catch (FileAlreadyExistsException x) {
			System.err.format("file named %s" +
					" already exists%n", file);
			dataToServer.writeBytes("error");
			dataToServer.flush();
		} catch (IOException x) {
			Files.delete(file);
			// Some other sort of failure, such as permissions.
			System.err.format("createFile error: %s%n", x);
		}
	}

	public static void readDirectory(String dataport, String filename) throws Exception{
		String dataline = "";
		/* setup data tcp connection */
		ServerSocket welcomeSocket = new ServerSocket(Integer.valueOf(dataport));
			//System.out.println("ready for data");
			Socket dataSocket = welcomeSocket.accept();
			String hostName = dataSocket.getInetAddress().getHostName();
			System.out.println("Receiving directory structure from " +
				hostName+":"+dataport + "\n");

			BufferedReader dataFromServer =
				new BufferedReader(new InputStreamReader(dataSocket.getInputStream()));
			dataline = dataFromServer.readLine();
			while (!dataline.contains("@@")){
				System.out.println(dataline);
				dataline = dataFromServer.readLine();
			}
			//return dataline;
	}

	/*
	public static void writeFile(String content, String filename){
		try{
			// Create new file
			String path="./"+filename;
			File file = new File(path);
			// If file doesn't exists, then create it
			if (!file.exists()) {
				file.createNewFile();
			}
			FileWriter fw = new FileWriter(file.getAbsoluteFile());
			BufferedWriter bw = new BufferedWriter(fw);
			// Write in file
			bw.write(content);
			// Close connection
			bw.close();
		}
		catch(Exception e){
			System.out.println(e);
		}
	}
	*/

}
