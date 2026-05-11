# 1. Base OS with Node.js
FROM node:20-bookworm-slim

# 2. Install GCC compiler and process tools (for killall)
RUN apt-get update && apt-get install -y gcc psmisc

# 3. Set the working directory
WORKDIR /usr/src/app

# 4. Copy Node package files and install express/socket.io
COPY package*.json ./
RUN npm install

# 5. Copy all your project files into the cloud container
COPY . .

# 6. Compile the C space engines exactly as they are named in your folder
RUN gcc -DNODE_ID=\"sat_a\" -DSTORE=\"./store_a\" -DTRACKER=\"a.txt\" -DBUNDLE_ID_START=2000 bundle.c socket_util.c generic_sat.c -o sat_a && \
    gcc -DNODE_ID=\"sat_b\" -DSTORE=\"./store_b\" -DTRACKER=\"b.txt\" -DBUNDLE_ID_START=3000 bundle.c socket_util.c generic_sat.c -o sat_b && \
    gcc -DNODE_ID=\"sat_c\" -DSTORE=\"./store_c\" -DTRACKER=\"c.txt\" -DBUNDLE_ID_START=4000 bundle.c socket_util.c generic_sat.c -o sat_c && \
    gcc bundle.c socket_util.c ground_station.c -o ground_station

# 7. Grant execution permissions to the compiled engines
RUN chmod +x sat_a sat_b sat_c ground_station

# 8. Expose the port
EXPOSE 3000

# 9. Ignite Mission Control
CMD ["node", "server.js"]