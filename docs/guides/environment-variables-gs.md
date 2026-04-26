# Ground Segment Environment Variables

## Remote Training Server

| Variable | Default    | Used By                                      | Description                                                    |
| -------- | ---------- | -------------------------------------------- | -------------------------------------------------------------- |
| `SERVER` | (required) | `upload_to_server.sh`, `download_weights.sh` | SSH address of the remote GPU server (e.g., `user@gpu-server`) |

## Receiver

`receiver.py` uses hardcoded values and does not read environment variables. See the [receiver configuration](../ground-segment/receiver.md#configuration) for details.

## Example: Training Workflow

```bash
export SERVER=saransh@gpu-server

# Upload dataset to server
cd ground_segment/data
bash upload_to_server.sh

# (train on server)

# Download weights after training
cd ../training
bash download_weights.sh
```
