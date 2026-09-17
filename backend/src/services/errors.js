export class ApiError extends Error {
  constructor(status, code, message) { super(message); this.status = status; this.code = code; }
}
export const invalid = (message = 'Invalid request') => { throw new ApiError(400, 'VALIDATION_ERROR', message); };
export const missing = () => { throw new ApiError(404, 'RESOURCE_NOT_FOUND', 'Resource not found'); };
export const conflict = (message = 'Invalid state transition', code = 'INVALID_STATE_TRANSITION') => { throw new ApiError(409, code, message); };
